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

#ifndef __PROCESS_COMMAND_H__
#define __PROCRSS_COMMAND_H__

#include "hddl_mediapipe_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @Synopsis  send register message.
 *
 * @Param hp        hddl_mediapipe
 *
 * @Returns TRUE  - if response is sent;
 *          FALSE - failed to sent response.
 */
gboolean send_register(mediapipe_hddl_impl_t* hp);

/**
 * @Synopsis  send metadata message.
 *
 * @Param hp        hddl_mediapipe
 *
 * @Returns TRUE  - if response is sent;
 *          FALSE - failed to sent response.
 */
gboolean send_metadata(mediapipe_hddl_impl_t* hp, const char* data, int len);

/**
 * @Synopsis  process one incoming message. If message is not in valid format it will be ignored
 *
 * @Param hp        hddl_mediapipe
 * @Param data      message data
 * @Param length    message length
 *
 * @Returns TRUE  - command run successfully;
 *          FALSE - command failed.
 */
gboolean process_message(mediapipe_hddl_impl_t* hp, const void* data, int length);

typedef enum {
    CREATE,
    DESTROY,
    MODIFY,
    PLAY,
    PAUSE,
    STOP
} msg_type;

/**
 * @Synopsis  Process one incoming message. If message is not in valid format it will be ignored
 *            Only request type or DESTROY is allowed, if not response will be sent with failed ret_code.
 *
 * @Param hp        hddl_mediapipe
 * @Param data      message data
 * @Param length    message length
 * @Param type      input: allowed message type, output: same message type or DESTROY if it's received
 *
 * @Returns TRUE  - command run successfully
 *          FALSE - command failed
 */
gboolean wait_message(mediapipe_hddl_impl_t* hp, const void* data, int length, msg_type* type);

#ifdef __cplusplus
}
#endif

#endif
