/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __HDDL_MEDIAPIPE_H__
#define __HDDL_MEDIAPIPE_H__

#ifdef __cplusplus
extern "C" {
#endif

#define HDDL_MEDIAPIPE_3

#include "mediapipe.h"

typedef struct mediapipe_hddl_s {
    mediapipe_t     mp;
    int             pipe_id;
} mediapipe_hddl_t;

mediapipe_hddl_t* hddl_mediapipe_setup(const char* server_uri, int pipe_id);
void hddl_mediapipe_run(mediapipe_hddl_t* hp);
void hddl_mediapipe_destroy(mediapipe_hddl_t* hp);

void hddl_mediapipe_send_metadata(mediapipe_hddl_t* hp, const char* data, int len);

#ifdef __cplusplus
}
#endif

#endif
