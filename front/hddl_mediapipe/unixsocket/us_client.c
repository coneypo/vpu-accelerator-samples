/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <glib.h>
#include <stdbool.h>
#include <json-c/json.h>
#include <arpa/inet.h>
#include "local_debug.h"
#include "us_client.h"
#include "process_command.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BUFFER_SIZE 4096
#define HEADER_LEN 4
#define TYPE_LEN 4
#define MAX_EVENTS 2


static void item_free_func(gpointer data)
{
    MessageItem *item = (MessageItem *) data;
    if (item && (item->len > 0)) {
        g_free(item->data);
    }
    return;
}

static char* create_send_buffer(trans_protocol *msg)
{
    char *str = g_new(char, msg->package_len);
    *(uint32_t*)str = htonl(msg->package_len);
    *(uint32_t*)(str + 4) = htonl(msg->type);
    if(msg->package_len > 8) {
        memcpy(str + 8, msg->payload, msg->package_len - 8);
    }
    return str;
}

/**
 * @Synopsis Add message to send buffer and add EPOLLOUT event
 *
 * @Param us_client       Unix socket client
 * @Param msg             Message to send
 * @Param type            Message type
 *
 */
void usclient_msg_to_send_buffer(usclient *client, gchar *msg, gint len, int type)
{
    uint32_t msg_len = htonl(len + HEADER_LEN + TYPE_LEN);
    uint32_t msg_type = htonl(type);
    g_mutex_lock(&client->lock);
    g_string_append_len(client->send_buffer, (const char*)&msg_len, HEADER_LEN);
    g_string_append_len(client->send_buffer, (const char*)&msg_type, TYPE_LEN);
    g_string_append_len(client->send_buffer, msg, len);

    client->epoll.event.events |= EPOLLOUT;
    int ret = epoll_ctl(client->epoll.epfd, EPOLL_CTL_MOD, client->socket_fd, &client->epoll.event);
    if (ret == -1) {
        LOG_ERROR("set epoll events error: %s\n", strerror(errno));
    }
    g_mutex_unlock(&client->lock);
}


static int usclient_connect(const char *server_uri, const int pipe_id)
{
    int sockfd = -1;
    struct sockaddr_un serv_addr;

    sockfd = socket(AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    //Connect to server
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, server_uri);
    int ret = connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (ret < 0) {
        perror("ERROR connecting!");
        exit(1);
    }

    // Send pipe id to server.
    char pipe_id_str[10] = {0};
    snprintf(pipe_id_str, sizeof(pipe_id_str), "%d", pipe_id);
    uint32_t type = eCommand_Pipeid;
    trans_protocol msg;
    msg.package_len = strlen(pipe_id_str) + 8;
    msg.type = type;
    msg.payload = pipe_id_str;
    char* send_buffer = create_send_buffer(&msg);
    int n = send(sockfd, send_buffer, msg.package_len, 0);
    g_free(send_buffer);

    if (n != msg.package_len) {
        perror("ERROR writing to socket");
        exit(1);
    }
    return sockfd;
}

/**
 * Synopsis Parse received data, push parse result to queue or post message to bus
 *
 * @Param buffer           receive buffer, may contain several packages.
 *                         format: 0~3byte: package length
 *                                 4~7byte: command type
 *                                 7~package length byte: payload 
 * @Param length           total buffer length to parse.
 *
 * @Param message_queue    store the parsed data.
 * @Param bus              message bus
 */
static int parse(gchar *buffer, int length, GAsyncQueue *message_queue, GstBus *bus)
{
    uint32_t type = 0;
    uint32_t payload_len = 0;
    char*    payload = NULL;

    int cur_pos = 0;

    while (cur_pos < length) {
        int rest_len = length - cur_pos;
        if (rest_len < HEADER_LEN) {
            LOG_ERROR("not enough buffer, cur_pos = %d\n", cur_pos);
            break;
        }
        uint32_t package_len = ntohl(*((uint32_t *)(buffer + cur_pos)));

        if (rest_len < package_len) {
            LOG_ERROR("not enough payload, cur_pos = %d\n", cur_pos);
            break;
        }

        type = ntohl(*((uint32_t *)(buffer + cur_pos + HEADER_LEN)));
        payload_len = package_len - HEADER_LEN - TYPE_LEN;
        payload = buffer + cur_pos + HEADER_LEN + TYPE_LEN;
        char* payload_str = g_new0(char, payload_len + 1);
        memcpy(payload_str, payload, payload_len);
        payload_str[payload_len] = '\0';

        if (type == eCommand_Config || type == eCommand_Launch) {
            if (payload_len != 0) {
                MessageItem *item = g_new0(MessageItem, 1);
                item->data = NULL;
                item->type = type;
                item->len = payload_len + 1;
                item->data = g_new0(char, item->len);
                g_memmove(item->data, payload_str, item->len);
                g_async_queue_push(message_queue, item);
            }
        } else {
            GstStructure *s = gst_structure_new("process_message",
                    "command_type", G_TYPE_UINT, type,
                    "payload_len", G_TYPE_UINT, payload_len,
                    "payload", G_TYPE_STRING, payload_str,
                    NULL
                    );
            GstMessage *m = gst_message_new_application(NULL, s);
            gst_bus_post(bus, m);
        }

        g_free(payload_str);
        cur_pos += package_len;
    }
    return cur_pos;
} 

static int send_data(usclient *client, const char *buffer, uint32_t length)
{
    int len = 0;
    int send_len = 0;
    do {
        len = send(client->socket_fd, buffer, length - send_len, 0);
        if (len > 0) {
            buffer += len;
            send_len += len;
        }
    } while (len > 0 && send_len < length);

    return send_len;
}

static void recv_data(const int sockfd, GString* recv_buffer)
{
    int recv_len = 0;
    gchar buffer[4096]; 

    do {
        recv_len = recv(sockfd, buffer, sizeof(buffer), 0);
        if (recv_len > 0) {
            g_string_append_len(recv_buffer, buffer, recv_len);
        }
    } while (recv_len != -1 && recv_len != 0);
}

static void usclient_handle_send(usclient *client)
{
    g_mutex_lock(&client->lock);
    int send_len = send_data(client, client->send_buffer->str, client->send_buffer->len);
    // we have already sent all data and need not to wait to EPOLLOUT
    // if there's new data to send, it will add EPOLLOUT there.
    if (send_len == client->send_buffer->len) {
        client->epoll.event.events &= ~(EPOLLOUT);
    }

    int ret = epoll_ctl(client->epoll.epfd, EPOLL_CTL_MOD, client->socket_fd, &client->epoll.event);
    if (ret == -1) {
        LOG_ERROR("set epoll events error: %s\n", strerror(errno));
    }

    g_string_erase(client->send_buffer, 0, send_len);
    g_mutex_unlock(&client->lock);
}

static void usclient_handle_recv(const int sockfd, GString *recv_buffer, GAsyncQueue *message_queue, GstBus *bus)
{
    int pos = 0;
    recv_data(sockfd, recv_buffer);

    if (recv_buffer->len > 0) {
        pos = parse(recv_buffer->str, recv_buffer->len, message_queue, bus);
    }

    g_string_erase(recv_buffer, 0, pos);
}


static void *usclient_thread(void *us_client)
{
    usclient *client = (usclient *) us_client;
    GString *recv_buffer = g_string_new(NULL);

    client->socket_fd = usclient_connect(client->server_uri, client->pipe_id);
    LOG_DEBUG("connected to server %s\n", client->server_uri);

    int epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("create epoll fd failed\n");
        exit(1);
    }
    client->epoll.epfd = epfd;
    struct epoll_event events[MAX_EVENTS];
    client->epoll.event.data.fd = client->socket_fd;
    client->epoll.event.events = EPOLLIN | EPOLLET;

    int ret = epoll_ctl(client->epoll.epfd, EPOLL_CTL_ADD, client->socket_fd, &client->epoll.event);
    if (ret == -1) {
        perror("ctl add epoll failed\n");
        exit(1);
    }

    while (1) {
        int n = epoll_wait(client->epoll.epfd, events, 10, -1);

        for (int i = 0; i < n; i++) {
            if (events[i].events & EPOLLIN) {
                usclient_handle_recv(client->socket_fd, recv_buffer, client->message_queue, client->bus);
            } else if(events[i].events & EPOLLOUT) {
                usclient_handle_send(client);
            } else if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
                LOG_ERROR("epoll error\n");
                continue;
            }
        }
    }
}

/**
 * Synopsis Create unix socket client
 *
 * @Param server_uri       Server uri string
 * @Param pipe_id          Pipe id
 *
 * @Returns us_client      Unix socket client
 */
usclient *usclient_setup(const char *server_uri, const int pipe_id)
{
    pthread_t thid;
    
    usclient *us_client = (usclient *) g_new0(usclient, 1);
    if (!us_client) {
        g_print("Client struct malloc failure\n");
        exit(1);
    }

    GstBus *bus = gst_bus_new();
    us_client->bus = bus;
    us_client->pipe_id = pipe_id;
    snprintf(us_client->server_uri, URI_LEN, "%s", server_uri);
    us_client->message_queue = g_async_queue_new_full(item_free_func);
    us_client->send_buffer = g_string_new(NULL);
    g_mutex_init(&us_client->lock);

    int ret = pthread_create(&thid, NULL, usclient_thread, (void *) us_client);
    if (ret != 0) {
        perror("Failed to create thread\n");
        exit(1);
    }
    
    pthread_detach(thid);
    
    int times = 100;
    while (!(us_client->socket_fd > 0) && times-- > 0) {
        g_usleep(100);
    }
    return us_client;
}

/**
 * @Synopsis Pops data from queue. This function blocks
 *           until data becomes available.
 *
 * @Param us_client       Pointer to usclient
 *
 * @Returns item          Message from queue
 */
MessageItem *usclient_get_data(usclient *us_client)
{
    if (!us_client) {
        g_print("Failed to get command, invalid unix socket client!\n");
        return NULL;
    }
    g_print("Waiting for server command.\n");
    MessageItem *item = (MessageItem *) g_async_queue_pop(
                            us_client->message_queue);
    return item;
}

/**
 * @Synopsis Pops data from queue. This function returns NULL
 *           if no data is received in 1s.
 *
 * @Param us_client       Unix socket client which owns the queue
 *
 * @Returns item          Message from queue
 */
MessageItem *usclient_get_data_timed(usclient *us_client)
{
    if (!us_client) {
        g_print("Failed to get command, invalid unix socket client!\n");
        return NULL;
    }
    gint64 timeout_microsec = 1000000; //1s
    MessageItem *item = (MessageItem *) g_async_queue_timeout_pop(
                            us_client->message_queue,
                            timeout_microsec
                        );
    return item;
}


void usclient_free_item(MessageItem *item)
{
        item_free_func(item);
}


/**
 * @Synopsis Destroy the structure of usclient.
 *
 * @Param us_client       Pointer to usclient
 *
 */
void usclient_destroy(usclient *us_client)
{
    if (!us_client) {
        return;
    }
    
    if (us_client->socket_fd) {
        close(us_client->socket_fd);
    }
    
    
    if (us_client->message_queue) {
        g_async_queue_unref(us_client->message_queue);
    }

    g_string_free(us_client->send_buffer, TRUE);

    g_free(us_client);
}

#ifdef __cplusplus
};
#endif
