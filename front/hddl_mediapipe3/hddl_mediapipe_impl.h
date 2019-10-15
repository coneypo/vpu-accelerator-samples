/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __HDDL_MEDIAPIPE_IMPL_H__
#define __HDDL_MEDIAPIPE_IMPL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "hddl_mediapipe.h"
#include "us_client.h"


typedef struct mediapipe_hddl_impl_s {
    mediapipe_hddl_t    hp;
    usclient*           client;
    GstBus*             bus;
    guint               bus_watch_id;
    GAsyncQueue*        message_queue;
    gboolean            is_mp_created;
    gboolean            is_running;
} mediapipe_hddl_impl_t;

#ifdef __cplusplus
}
#endif

#endif
