/*
 *Copyright (C) 2018 Intel Corporation
 *
 *SPDX-License-Identifier: LGPL-2.1-only
 *
 *This library is free software; you can redistribute it and/or modify it under the terms
 * of the GNU Lesser General Public License as published by the Free Software Foundation;
 * version 2.1.
 *
 *This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with this library;
 * if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __BLEND_WRAPPER_H__
#define __BLEND_WRAPPER_H__

#include "boundingbox.h"
#include <gst/gstbuffer.h>
#include <gst/gstpad.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _cvdl_handler {
    GstBufferPool* mOsdPool;
    void* mImgProcessor;
    int mImageWidth;
    int mImageHeight;
    gboolean mInited;
} CvdlHandler;

typedef CvdlHandler* FrameHandler;

FrameHandler cvdlhandler_create();
void cvdlhandler_destroy(FrameHandler handle);
void cvdlhandler_init(FrameHandler handle, GstCaps* caps, const char* ocl_format);
GstBuffer* cvdlhandler_get_free_buffer(FrameHandler handle);

void cvdlhandler_generate_osd(FrameHandler handle, BoundingBox* boxList, gint size, GstBuffer** osd_buf);
void cvdlhandler_process_osd(FrameHandler handle, GstBuffer* buffer, GstBuffer* osd_buf);
void cvdlhandler_process_boundingbox(FrameHandler handle, GstBuffer* buffer, BoundingBox* box, guint32 num);
void cvdlhandler_crop_frame(FrameHandler handle, GstBuffer* buffer, BoundingBox* box, guint32 num);

#ifdef __cplusplus
}
#endif
#endif
