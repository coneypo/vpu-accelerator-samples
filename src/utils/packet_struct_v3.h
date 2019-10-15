/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <inttypes.h>

#define MAGIC_CHAR      'K'
#define PACKET_VERSION  3

//////////////////////////////////////////////////////////////////////
/// Stream Channel
///

#define PACKET_SEND_HEVC        0x01
#define PACKET_SEND_JPEG        0x02
#define PACKET_SEND_AVC         0x03
#define PACKET_RECEIVE_HEVC     0x10
#define PACKET_RECEIVE_JPEG     0x20
#define PACKET_RECEIVE_AVC      0x30

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
    uint8_t packet_type;    // PACKET_SEND_HEVC/_JPEG/_AVC, PACKET_RECEIVE_HEVC/_JPEG/_AVC
    uint8_t stream_id;      // Ignored.
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
                                   // | Header    | Meta     | Results  | Jpeg0   | Jpeg1   |...
                                   // |<- base line of offset.
    uint32_t jpeg_size;            // jpeg binary size (bytes)
    uint32_t reserved2;
    uint16_t left;
    uint16_t top;
    uint16_t width;
    uint16_t height;
    uint32_t reserved3;
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

