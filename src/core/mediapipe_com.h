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


#define mp_string(str)     { sizeof(str) - 1, (u_char *) str }
#define mp_null_string     { 0, NULL }



#include "../utils/mp_utils.h"
#include "../utils/local_debug.h"
#include "../utils/mp_cairo.h"
#include "../config/mp_config.h"
#include "../config/mp_config_json.h"
#include "mp_module.h"
#include "mediapipe.h"
#include "mp_branch.h"

#ifdef __cplusplus
}
#endif

#endif
