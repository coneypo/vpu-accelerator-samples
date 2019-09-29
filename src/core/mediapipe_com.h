/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __MEDIAPIPE_COM_H__
#define __MEDIAPIPE_COM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/video.h>
#include <malloc.h>
#include <gst/gstallocator.h>
#include <gst/allocators/allocators.h>


typedef enum MEDIAPIPE_STATE_E {
    STATE_NOT_CREATE = 0,
    STATE_READY,
    STATE_START,
    STATE_NUM
} MEDIAPIPE_STATE;

typedef struct mediapipe_s mediapipe_t;
typedef struct mp_module_s          mp_module_t;
typedef struct mp_conf_s            mp_conf_t;
typedef struct mp_command_s            mp_command_t;

typedef intptr_t        mp_int_t;
typedef uintptr_t       mp_uint_t;
typedef intptr_t        mp_flag_t;

#define mp_version      2000000
#define MP_VERSION      "2.00.0"
#define MP_VER          "mediapipe/" MP_VERSION
#define MP_CORE_MODULE       0x4d123456

#define  MP_OK          0
#define  MP_ERROR      -1
#define  MP_AGAIN      -2
#define  MP_BUSY       -3
#define  MP_DONE       -4
#define  MP_DECLINED   -5
#define  MP_ABORT      -6
#define  MP_IGNORE     -7

#define MP_CONF_OK          NULL
#define MP_CONF_ERROR       (void *) -1


#define mp_string(str)     { sizeof(str) - 1, (const char *) str }
#define mp_null_string     { 0, NULL }



#include "../utils/mp_utils.h"
#include "../utils/local_debug.h"
#include "../utils/mp_cairo.h"
#include "../config/mp_config.h"
#include "../config/mp_config_json.h"
#include "mp_module.h"
#include "mediapipe.h"
#include "mp_branch.h"

#ifdef DRM_TYPE
#include "mp_gstdrmbomemory.h"
#endif

#if defined(VPUSMM) || defined(USE_VPUSMM)
#include "gstvpusmm.h"
#endif

#ifdef __cplusplus
}
#endif

#endif
