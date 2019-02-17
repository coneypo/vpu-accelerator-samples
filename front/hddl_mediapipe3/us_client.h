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

struct _epoll {
    struct epoll_event event;
    int epfd;
};

typedef void (*recv_callback)(void* private_data, void* data, int len);

struct _usclient {
    int             socket_fd;
    pthread_t       tid;
    char            server_uri[URI_LEN];
    GString*        send_buffer;
    recv_callback   recv_callback;
    void*           private_data;
    GMutex          lock;
    struct _epoll   epoll;
};

typedef struct _usclient usclient;

/**
 * Synopsis Create unix socket client
 *
 * @Param server_uri       Server uri string
 * @Param recv_cb          incoming data process callback function
 *
 * @Returns us_client      Unix socket client
 */
usclient* usclient_setup(const char* server_uri, recv_callback recv_cb, void* private_data);

/**
 * @Synopsis Destroy the structure of usclient.
 *
 * @Param us_client       Pointer to usclient
 *
 */
void usclient_destroy(usclient* us_client);

/**
 * @Synopsis Add message to send buffer and add EPOLLOUT event
 *
 * @Param us_client       Unix socket client
 * @Param msg             Message to send
 * @Param type            Message type
 *
 */
void usclient_send_async(usclient* client, const void* msg, int len);

/**
 * @Synopsis Send message synchronously
 *
 * @Param us_client       Unix socket client
 * @Param msg             Message to send
 * @Param type            Message type
 *
 */
int usclient_send_sync(usclient* client, const void* msg, int len);

#ifdef __cplusplus
};
#endif
#endif
