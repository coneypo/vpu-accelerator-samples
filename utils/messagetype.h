/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef MESSAGETYPE
#define MESSAGETYPE

enum MessageType {
    MESSAGE_UNKNOWN = 0,
    MESSAGE_WINID,
    MESSAGE_STRING,
    MESSAGE_IMAGE,
    MESSAGE_BYTEARRAY,
    MESSAGE_ACTION
};

enum PipelineAction {
    MESSAGE_START,
    MESSAGE_STOP,
    MESSAGE_REPLAY
};

#endif // MESSAGETYPE
