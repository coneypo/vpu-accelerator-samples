/* * MIT License
 *
 * Copyright (c) 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef __US_CLIENT_H__
#define __US_CLIENT_H__

#include <gst/gst.h>
#include <sys/epoll.h>

#ifdef __cplusplus
extern "C" {
#endif

#define URI_LEN 200

typedef struct _MessageItem {
    char *data;
    gint type;
    gint len;
} MessageItem;

struct _epoll {
   struct epoll_event event;
   int epfd;
};

struct _usclient {
    int socket_fd;
    int pipe_id;
    int tid;
    char server_uri[URI_LEN];
    GstBus *bus;
    GAsyncQueue *message_queue;
    GString *send_buffer;
    GMutex lock; 
    struct _epoll epoll;
};

typedef struct _usclient usclient;

typedef struct _transfer_protocol {
    uint32_t package_len;
    uint32_t type;
    char *payload;
} trans_protocol;

usclient *usclient_setup(const char *server_uri, const int pipe_id);
void usclient_msg_to_send_buffer(usclient *client, gchar *msg, gint len, int type);
MessageItem *usclient_get_data(usclient *us_client);
MessageItem *usclient_get_data_timed(usclient *us_client);
void usclient_destroy(usclient *us_client);
void usclient_free_item(MessageItem *item);


#ifdef __cplusplus
};
#endif
#endif
