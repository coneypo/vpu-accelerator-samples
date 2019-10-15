/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <inttypes.h>

#define MAGIC_CHAR      'K'
#define PACKET_VERSION  1

//////////////////////////////////////////////////////////////////////
/// Stream Channel
///

#define PACKET_SEND_HEVC        0x01
#define PACKET_SEND_JPEG        0x02
#define PACKET_RECEIVE_HEVC     0x10
#define PACKET_RECEIVE_JPEG     0x20

typedef struct _Header
{
    uint8_t magic;
    uint8_t version;
    uint16_t meta_size;     // Meta struct size (bytes)
    uint32_t package_size;  // VideoPacket or ResultPacket size (bytes)
} Header;

typedef struct _Meta
{
    uint8_t version;
    uint8_t packet_type;    // PACKET_SEND_HEVC, PACKET_SEND_JPEG, PACKET_RECEIVE_HEVC, PACKET_RECEIVE_JPEG
    uint8_t stream_id;
    uint8_t num_rois;
    uint32_t frame_number;
} Meta;

typedef struct _ROI
{
    uint8_t reserved;
    uint8_t object_index;
    uint16_t classification_index; // expected GT value for algo validation
    uint16_t left;
    uint16_t top;
    uint16_t width;
    uint16_t height;
    uint32_t reserved2;
} ROI;

typedef struct _ClassificationResult
{
    uint8_t reserved1;
    uint8_t object_index;
    uint16_t classification_index; // classified id from GVAClassify
    uint32_t starting_offset;      // jpeg starting offset based on packet header.
                                   // | Header    | Meta     | Jpeg0   | Jpeg1   |...
                                   // |<- base line of offset.
    uint32_t jpeg_size;            // jpeg binary size (bytes)
    uint32_t reserved2;
} ClassificationResult;

typedef struct _VideoPacket
{
    Header header;
    Meta meta;
    ROI* rois;
    uint8_t* encoded_frame;
} VideoPacket;

typedef struct _ResultPacket
{
    Header header;
    Meta meta;
    ClassificationResult* results;
    uint8_t* jpegs;
} ResultPacket;



//////////////////////////////////////////////////////////////////////
/// Control Channel
///

#define CHANNEL_CMD_INIT        0x01
#define CHANNEL_CMD_DEINIT      0x02

#define CHANNEL_STS_IDLE        0x00        // xlink is initialized.
#define CHANNEL_STS_READY       0x01        // control channel is opened.
#define CHANNEL_STS_RUNNING     0x02        // pipeline is ready and stream channels are opened.
#define CHANNEL_STS_ERROR       0x03        // Error is occurred in pipeline or arm-side app. XLink error should be managed in xlink apis.
                                            // If Error state, host-side app will deinitialize all channels and try to open again.

typedef struct _ChannelInit
{
    uint8_t channel_id;
    uint8_t stream_id;
    uint8_t stream_format;      // 0: hevc, 1: jpeg
    uint8_t reserved;
} ChannelInit;

typedef struct _ChannelCommand
{
    uint8_t magic;
    uint8_t version;
    uint8_t stream_num;
    uint8_t command;        // CMD_INIT_CHANNEL / CHANNEL_CMD_DEINIT
    uint8_t* command_data;  // struct ChannelInit / if unnecessary, it should be null.
} ChannelCommand;

typedef struct _ChannelResponse
{
    uint8_t magic;
    uint8_t version;
    uint8_t state;          // CHANNEL_STS_IDLE, CHANNEL_STS_READY, CHANNEL_STS_RUNNING, CHANNEL_STS_ERROR
    uint8_t message_size;   // message string size
    uint8_t* message_data;  // message string
} ChannelResponse;
