
#ifndef __US_CLIENT_H__
#define __US_CLIENT_H__

#include <gst/gst.h>

#ifdef __cplusplus
extern "C" {
#endif

#define URI_LEN 200

typedef struct _MessageItem {
    char *data;
    gint type;
    gint len;
} MessageItem;

struct _usclient {
    int socket_fd;
    int pipe_id;
    int tid;
    char server_uri[URI_LEN];
    GstBus *bus;
    GAsyncQueue *message_queue;
};

typedef struct _usclient usclient;

typedef struct _transfer_protocol {
    uint32_t package_len;
    uint32_t type;
    char *payload;
} trans_protocol;

usclient *usclient_setup(const char *server_uri, const int pipe_id);
MessageItem *usclient_get_data(usclient *us_client);
MessageItem *usclient_get_data_timed(usclient *us_client);
void usclient_destroy(usclient *us_client);
void usclient_free_item(MessageItem *item);


#ifdef __cplusplus
};
#endif
#endif
