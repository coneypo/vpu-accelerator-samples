#ifndef __HDDL_MEDIAPIPE_H__
#define __HDDL_MEDIAPIPE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "mediapipe.h"
#include "unixsocket/us_client.h"


typedef struct {
    mediapipe_t     *mp;
    gint            pipe_id;
    usclient        *client;
    GThread         *message_handle_thread;
} mediapipe_hddl_t;


#ifdef __cplusplus
}
#endif

#endif
