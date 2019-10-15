/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "us_client.h"
#include "local_debug.h"
#include <arpa/inet.h>
#include <errno.h>
#include <glib.h>
#include <json-c/json.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUFFER_SIZE 4096
#define HEADER_LEN 4
#define TYPE_LEN 4
#define MAX_EVENTS 2

void usclient_send_async(usclient* client, const void* msg, int len)
{
    g_mutex_lock(&client->lock);
    uint32_t net_len = htonl(len);
    g_string_append_len(client->send_buffer, (const char*)&net_len, sizeof(net_len));
    g_string_append_len(client->send_buffer, msg, len);

    client->epoll.event.events |= EPOLLOUT;
    int ret = epoll_ctl(client->epoll.epfd, EPOLL_CTL_MOD, client->socket_fd, &client->epoll.event);
    if (ret == -1) {
        LOG_ERROR("set epoll events error: %s\n", strerror(errno));
    }
    g_mutex_unlock(&client->lock);
}

static int usclient_connect(const char* server_uri)
{
    int sockfd = -1;
    struct sockaddr_un serv_addr;

    sockfd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    //Connect to server
    bzero((char*)&serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, server_uri);
    int ret = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (ret < 0) {
        perror("ERROR connecting!");
        exit(1);
    }

    return sockfd;
}

/**
 * Synopsis Parse received data, push parse result to queue or post message to bus
 *
 * @Param buffer           receive buffer, may contain several packages.
 * @Param length           total buffer length to parse.
 * @Param client           usclient pointer.
 *
 */
static int parse(gchar* buffer, int length, usclient* client)
{
    uint32_t payload_len = 0;
    char* payload = NULL;

    int cur_pos = 0;

    while (cur_pos < length) {
        int rest_len = length - cur_pos;
        if (rest_len < sizeof(uint32_t)) {
            LOG_ERROR("not enough buffer, cur_pos = %d\n", cur_pos);
            break;
        }
        uint32_t package_len = ntohl(*((uint32_t*)(buffer + cur_pos)));

        cur_pos += sizeof(uint32_t);

        if (rest_len < package_len) {
            LOG_ERROR("not enough payload, cur_pos = %d\n", cur_pos);
            break;
        }

        payload_len = package_len;
        payload = buffer + cur_pos;
        char* payload_str = g_new0(char, payload_len);
        if (!payload_str) {
            LOG_ERROR("not enough memory, cur_pos = %d\n", cur_pos);
            continue;
        }
        memcpy(payload_str, payload, payload_len);

        if (client->recv_callback)
            client->recv_callback(client->private_data, payload_str, payload_len);

        cur_pos += package_len;
    }
    return cur_pos;
}

static int send_data(usclient* client, const void* buffer, uint32_t length)
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

static void usclient_handle_send(usclient* client)
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

static void usclient_handle_recv(usclient* client, GString* recv_buffer)
{
    int pos = 0;
    recv_data(client->socket_fd, recv_buffer);

    if (recv_buffer->len > 0) {
        pos = parse(recv_buffer->str, recv_buffer->len, client);
    }

    g_string_erase(recv_buffer, 0, pos);
}

static void* usclient_thread(void* us_client)
{
    usclient* client = (usclient*)us_client;
    GString* recv_buffer = g_string_new(NULL);

    client->socket_fd = usclient_connect(client->server_uri);
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
                usclient_handle_recv(client, recv_buffer);
            } else if (events[i].events & EPOLLOUT) {
                usclient_handle_send(client);
            } else if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
                LOG_ERROR("epoll error\n");
                continue;
            }
        }
    }
}

usclient* usclient_setup(const char* server_uri, recv_callback recv_cb, void* private_data)
{
    pthread_t thid;

    usclient* us_client = (usclient*)g_new0(usclient, 1);
    if (!us_client) {
        g_print("Client struct malloc failure\n");
        return NULL;
    }

    snprintf(us_client->server_uri, URI_LEN, "%s", server_uri);
    us_client->send_buffer = g_string_new(NULL);
    us_client->recv_callback = recv_cb;
    us_client->private_data = private_data;
    g_mutex_init(&us_client->lock);

    int ret = pthread_create(&thid, NULL, usclient_thread, (void*)us_client);
    if (ret != 0) {
        perror("Failed to create thread\n");
        usclient_destroy(us_client);
        return NULL;
    }

    pthread_detach(thid);

    us_client->tid = thid;

    int times = 100;
    while (!(us_client->socket_fd > 0) && times-- > 0) {
        g_usleep(100);
    }
    return us_client;
}

void usclient_destroy(usclient* us_client)
{
    if (!us_client) {
        return;
    }

    if (us_client->socket_fd) {
        close(us_client->socket_fd);
    }

    g_string_free(us_client->send_buffer, TRUE);

    g_free(us_client);
}

int usclient_send_sync(usclient* client, const void* msg, int len)
{
    if (len <= 0)
        return -1;
    uint32_t net_len = htonl(len);
    if (send_data(client, &net_len, sizeof(uint32_t)) != sizeof(uint32_t))
        return -1;
    if (send_data(client, msg, (uint32_t)len) != len)
        return -1;
    return len;
}

#ifdef __cplusplus
};
#endif
