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


#ifndef __IMAGE_PROCESSOR_H__
#define __IMAGE_PROCESSOR_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/gstbuffer.h>
#include <gst/gstpad.h>

#include <interface/videodefs.h>
#include <interface/vppinterface.h>

using namespace HDDLStreamFilter;

enum {
    IMG_PROC_TYPE_OCL_BLENDER = 0, /* ocl blender */
    IMG_PROC_TYPE_OCL_INPLACE_BLENDER = 1, /* ocl inplace blender */
    IMG_PROC_TYPE_OCL_CROP = 2, /* ocl crop*/
};

class ImageProcessor {
public:
    ImageProcessor();
    ~ImageProcessor();

    /* parse input caps and create ocl buffer pool based on ocl caps
    *  vppType: Blender, Inplace Blender and Crop
    */
    GstFlowReturn init(GstCaps *incaps, int vppType);

    /*Process image:
    *   Note: oclcontext will be setup when first call this function
    */
    GstFlowReturn process_image(GstBuffer* inbuf, GstBuffer* inbuf2, GstBuffer** outbuf, VideoRect *crop);

    GstFlowReturn process_image(GstBuffer* inbuf, BoundingBox* box, guint32 num);

    GstFlowReturn process_image(GstBuffer* inbuf,std::shared_ptr<cv::UMat> outMat, VideoRect *crop);


    void get_input_video_size(int *w, int *h)
    {
        *w = mInVideoInfo.width;
        *h = mInVideoInfo.height;
    }

    void ocl_lock();
    void ocl_unlock();

    GstVideoInfo     mInVideoInfo;

    SharedPtr<VideoFrame> mSrcFrame;
    SharedPtr<VideoFrame> mSrcFrame2;
    SharedPtr<VideoFrame> mDstFrame;

    SharedPtr<VppInterface> mOclVpp;
    int mOclVppType;

private:
    void setup_ocl_context(VideoDisplayID display);
    void setup_ocl_context_output(VideoDisplayID display);


    GstFlowReturn process_image_blend(GstBuffer* inbuf,GstBuffer* inbuf2, GstBuffer** outbuf, VideoRect *rect);
    GstFlowReturn process_image_blend_inplace(GstBuffer* inbuf, GstBuffer* osd_buf, VideoRect *crop);
    GstFlowReturn process_image_crop(GstBuffer* inbuf, GstBuffer** outbuf, VideoRect *rect);

    SharedPtr<OclContext> mContext;
    gboolean   mOclInited;
};
#endif
