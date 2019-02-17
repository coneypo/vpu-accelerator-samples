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
