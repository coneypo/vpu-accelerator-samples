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

#include "us_client.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BUFFER_SIZE 1024

static void item_free_func(gpointer data)
{
    MessageItem *item = (MessageItem *) data;
    if (item && (item->len > 0)) {
        g_free(item->data);
    }
    return;
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
    int type_id = 1; 
    struct json_object *obj = json_object_new_object();
    struct json_object *pipe = json_object_new_int(pipe_id);
    struct json_object *type = json_object_new_int(type_id);
    json_object_object_add(obj, "type", type);
    json_object_object_add(obj, "payload", pipe);
   
    const char *send_str = json_object_to_json_string(obj);
    printf("send_str = %s\n", send_str);
    int n = write(sockfd, send_str, strlen(send_str));
    json_object_put(obj);
    if (n < 0) {
        perror("ERROR writing to socket");
        exit(1);
    }
    
    return sockfd;
}


static MessageItem *usclient_recv(const int sockfd)
{
    int continue_recv = 1;
    int size = 0;
    int total_len = 0;
    int recv_index = 1;
    int recv_len = 0;
    
    gchar *buffer = g_new0(char, BUFFER_SIZE);
    MessageItem *item = g_new0(MessageItem, 1);
    
    while (continue_recv) {
        recv_len = recv(sockfd, buffer + total_len, BUFFER_SIZE, 0);
        if ((recv_len == -1)||(recv_len == 0)) {
            break;
        }
        
        recv_index++;
        total_len += recv_len;
        
        if (recv_len == BUFFER_SIZE) {
            buffer = g_renew(char, buffer, recv_index * BUFFER_SIZE);
            continue_recv = 1;
        } else {
            continue_recv = 0;
        }
    }

    if(total_len == 0) {
        g_free(buffer);
        g_free(item);
        return NULL;
    } else {
        item->data = buffer;
        item->len = total_len;
        return item;
    }
}


static void *usclient_thread(void *us_client)
{
    usclient *client = (usclient *) us_client;
    
    client->socket_fd = usclient_connect(client->server_uri, client->pipe_id);

    while (true) {
        MessageItem *item =  usclient_recv(client->socket_fd);
        if (item != NULL) {
            g_async_queue_push(client->message_queue, item);
        }
    }
}

/**
 * Synopsis Create unix socket client, connect to server,
 *          push received data to queue.
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
    us_client->pipe_id = pipe_id;
    snprintf(us_client->server_uri, URI_LEN, "%s", server_uri);
    us_client->message_queue = g_async_queue_new_full(item_free_func);
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
 *           if no data is received in 400ms.
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
    gint64 timeout_microsec = 400000; //400ms
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

    g_free(us_client);
}

#ifdef __cplusplus
};
#endif
