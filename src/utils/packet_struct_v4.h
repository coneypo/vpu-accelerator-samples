/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <inttypes.h>

#define MAGIC_CHAR      'K'
#define PACKET_VERSION  4

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
    uint8_t      version;            // TBH packet version starts from 20
    uint8_t      packet_type;   /* to be updated from xBay when responding */
    uint16_t     reserved;
    uint32_t     stream_id;
    uint32_t     num_rois;      /* to be updated from xBay as a result of video analytics*/
    uint32_t     frame_number;
    int64_t     ts_pipeline_start;   /* to be updated from xBay as a result of VA pipeline */
    int64_t     ts_pipeline_end;    /* to be updated from xBay as a result of VA pipeline */
} Meta;

typedef struct _DetectionResult
{
    uint16_t     left;
    uint16_t     top;
    uint16_t     width;
    uint16_t     height;
    uint32_t     num_classifications;
    uint32_t     tracking_id; /* 0 when OT did not run */
    uint32_t     jpeg_offset;      // jpeg starting offset based on packet header.
    // | Header    | Meta     | Results  | Jpeg0   | Jpeg1   |...
    // |<- base line of offset.
    uint32_t     jpeg_size;            // jpeg binary size (bytes)
    uint16_t     det_label_id;
    uint16_t     det_prob;  /* *1000 from original result */
} DetectionResult;

typedef struct _ClassificationResult
{
    uint16_t     cls_label_id; // classified id from GVAClassify
    uint8_t      cls_nn_name[14]; // null-terminated, truncated after 13 bytes
    uint16_t     cls_prob;  /* *1000 from original result */
    uint16_t     reserved;
} ClassificationResult;

typedef struct _FrameResult
{
    DetectionResult det_result;
    ClassificationResult cls_result;
} FrameResult;

typedef struct _VideoPacket
{
    Header header;
    Meta meta;
    uint8_t* encoded_frame;
} VideoPacket;

typedef struct _ResultPacket
{
    Header header;
    Meta meta;
    FrameResult* results;
    uint8_t* jpegs;
} ResultPacket;

