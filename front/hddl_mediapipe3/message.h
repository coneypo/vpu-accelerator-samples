/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
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
 * @Synopsis  send eos message.
 *
 * @Param hp        hddl_mediapipe
 *
 * @Returns TRUE  - if response is sent;
 *          FALSE - failed to sent response.
 */
gboolean send_eos(mediapipe_hddl_impl_t* hp);

/**
 * @Synopsis  send error message.
 *
 * @Param hp        hddl_mediapipe
 *
 * @Returns TRUE  - if response is sent;
 *          FALSE - failed to sent response.
 */
gboolean send_error(mediapipe_hddl_impl_t* hp);

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
