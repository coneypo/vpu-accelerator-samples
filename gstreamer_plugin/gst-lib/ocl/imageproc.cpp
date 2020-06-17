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
/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include <gstmfxdisplay.h>
#include <gstmfxsurface.h>
#include <gstmfxsurface_vaapi.h>
#include <mutex>


#include "boundingbox.h"
#include "imageproc.h"
#include "oclpool.h"
#include "oclutils.h"
#include "video-format.h"
#include "vpphost.h"


using namespace HDDLStreamFilter;
using namespace std;
// vpp lock for ocl context
static std::mutex vpp_mutext;

ImageProcessor::ImageProcessor()
{
    mSrcFrame.reset(g_new0(VideoFrame, 1), g_free);
    mSrcFrame2.reset(g_new0(VideoFrame, 1), g_free);
    mDstFrame.reset(g_new0(VideoFrame, 1), g_free);

    gst_video_info_init(&mInVideoInfo);

    mOclInited = false;
}

ImageProcessor::~ImageProcessor()
{
    // mSrcFrame/mDstFrame will be relseased by SharedPtr automatically
    // OCL context will be done in OclVppBase::~OclVppBase ()
}
GstFlowReturn ImageProcessor::init(GstCaps* incaps, int vppType)
{
    if (!incaps) {
        GST_ERROR("Failed to init ImageProcessor: no in caps specified");
        return GST_FLOW_ERROR;
    }

    if (!gst_video_info_from_caps(&mInVideoInfo, incaps)) {
        GST_ERROR("Failed to init ImageProcessor: no incaps info found!");
        return GST_FLOW_ERROR;
    }

    mOclVppType = vppType;
    return GST_FLOW_OK;
}

void ImageProcessor::setup_ocl_context(VideoDisplayID display)
{
    if (mOclInited)
        return;

    mContext = OclContext::create(display);
    if (!SHARED_PTR_IS_VALID(mContext)) {
        GST_ERROR("oclcrc: failed to create ocl ctx");
        return;
    }

    switch (mOclVppType) {
    case IMG_PROC_TYPE_OCL_BLENDER:
        mOclVpp.reset(NEW_VPP_SHARED_PTR(OCL_VPP_MYBLENDER));
        break;

    case IMG_PROC_TYPE_OCL_CROP:
        mOclVpp.reset(NEW_VPP_SHARED_PTR(OCL_VPP_MYCONVERTOR));
        break;

    default:
        GST_ERROR("ocl: invalid vpp type!!!\n");
        break;
    }

    if (!SHARED_PTR_IS_VALID(mOclVpp) || (OCL_SUCCESS != mOclVpp->setOclContext(mContext))) {
        GST_ERROR("oclcrc: failed to init ocl_vpp");
        mOclVpp.reset();
    }

    GST_LOG("oclcrc: success to init ocl_vpp\n");
    mOclInited = true;
}

void ImageProcessor::ocl_lock()
{
    vpp_mutext.lock();
}

void ImageProcessor::ocl_unlock()
{
    vpp_mutext.unlock();
}

/* blend cvdl osd on orignal NV12 surface
 *    input: nv12 buffer
 *    osd_buf: gray8 buffer
 *    rect: the size of osd buffer
 */
GstFlowReturn ImageProcessor::process_image_blend(GstBuffer* inbuf, GstBuffer* osd_buf, VideoRect* crop)
{

    GstMfxSurface *surface = NULL, *newSurface = NULL;
    GstMfxVideoMeta* mfxMeta = NULL;
    GstMfxDisplay* mfxDisplay = NULL;
    VideoDisplayID display;
    VASurfaceID surface_id, new_surface_id;
    OclMemory* osd_mem;

    mfxMeta = gst_buffer_get_mfx_video_meta(inbuf);
    surface = gst_mfx_video_meta_get_surface(mfxMeta);
    surface_id = (VASurfaceID)(gst_mfx_surface_get_id(surface));
    mfxDisplay = gst_mfx_surface_vaapi_get_display(surface);
    display = gst_mfx_display_get_vadisplay(mfxDisplay);

    static GstMfxSurfacePool* surfacePool = gst_mfx_surface_pool_new(mfxDisplay, &mInVideoInfo, false);
    newSurface = gst_mfx_surface_new_from_pool(surfacePool);
    new_surface_id = (VASurfaceID)(gst_mfx_surface_get_id(newSurface));

    /* input data must be NV12 surface from mfxdec element */
    mSrcFrame->fourcc = video_format_to_va_fourcc(GST_VIDEO_INFO_FORMAT(&mInVideoInfo));
    mSrcFrame->surface = surface_id;
    mSrcFrame->width = mInVideoInfo.width;
    mSrcFrame->height = mInVideoInfo.height;

    if (mSrcFrame->surface == INVALID_SURFACE_ID) {
        GST_ERROR("Failed to get VASurface!");
        return GST_FLOW_ERROR;
    }

    setup_ocl_context(display);

    if (mSrcFrame->fourcc != OCL_FOURCC_NV12) {
        GST_ERROR("only support RGBA blending on nv12 video frame");
        return GST_FLOW_ERROR;
    }

    // osd memory is OCL buffer
    osd_mem = ocl_memory_acquire(osd_buf);
    if (!osd_mem) {
        GST_ERROR("Failed to acquire ocl memory from osd_buf");
        return GST_FLOW_ERROR;
    }

    mSrcFrame2->fourcc = osd_mem->fourcc;
    mSrcFrame2->mem = osd_mem->mem;
    mSrcFrame2->width = crop->width;
    mSrcFrame2->height = crop->height;

    mDstFrame->fourcc = video_format_to_va_fourcc(GST_VIDEO_INFO_FORMAT(&mInVideoInfo));
    mDstFrame->surface = new_surface_id;
    mDstFrame->width = mInVideoInfo.width;
    mDstFrame->height = mInVideoInfo.height;

    //ocl_lock();
    OclStatus status = mOclVpp->process(mSrcFrame, mSrcFrame2, mDstFrame);
    //ocl_unlock();

    gst_mfx_display_unref(mfxDisplay);
    gst_mfx_video_meta_set_surface(mfxMeta, newSurface);

    if (status == OCL_SUCCESS) {
        return GST_FLOW_OK;
    } else {
        return GST_FLOW_ERROR;
    }
}

GstFlowReturn ImageProcessor::process_image_crop(GstBuffer* inbuf, GstBuffer** out_buf, VideoRect* rect)
{

    GstMfxSurface* surface = NULL;
    GstMfxVideoMeta* mfxMeta = NULL;
    GstMfxDisplay* mfxDisplay = NULL;
    VideoDisplayID display;
    VASurfaceID surface_id;
    GstBuffer* dst_buf;
    OclMemory* dst_mem;
    dst_buf = *out_buf;

    mfxMeta = gst_buffer_get_mfx_video_meta(inbuf);
    surface = gst_mfx_video_meta_get_surface(mfxMeta);
    surface_id = (VASurfaceID)(gst_mfx_surface_get_id(surface));
    mfxDisplay = gst_mfx_surface_vaapi_get_display(surface);
    display = gst_mfx_display_get_vadisplay(mfxDisplay);
    gst_mfx_display_unref(mfxDisplay);

    /* input data must be NV12 surface from mfxdec element */
    mSrcFrame->fourcc = video_format_to_va_fourcc(GST_VIDEO_INFO_FORMAT(&mInVideoInfo));
    mSrcFrame->surface = surface_id;
    mSrcFrame->width = mInVideoInfo.width;
    mSrcFrame->height = mInVideoInfo.height;

    if (mSrcFrame->surface == INVALID_SURFACE_ID) {
        GST_ERROR("Failed to get VASurface!");
        return GST_FLOW_ERROR;
    }

    setup_ocl_context(display);

    if (mSrcFrame->fourcc != OCL_FOURCC_NV12) {
        GST_ERROR("only support RGBA blending on nv12 video frame");
        return GST_FLOW_ERROR;
    }

    // dst memory is OCL buffer
    dst_mem = ocl_memory_acquire(dst_buf);
    if (!dst_mem) {
        GST_ERROR("Failed to acquire ocl memory from osd_buf");
        return GST_FLOW_ERROR;
    }

    //dst buf is bgra
    mDstFrame->fourcc = dst_mem->fourcc;
    mDstFrame->mem = dst_mem->mem;
    mDstFrame->width = rect->width;
    mDstFrame->height = rect->height;

    ocl_lock();
    OclStatus status = mOclVpp->process(mSrcFrame, mDstFrame);
    ocl_unlock();

    if (status == OCL_SUCCESS) {
        return GST_FLOW_OK;
    } else {
        return GST_FLOW_ERROR;
    }

    return GST_FLOW_OK;
}

GstFlowReturn ImageProcessor::process_image(GstBuffer* inbuf,
    GstBuffer* inbuf2, GstBuffer** outbuf, VideoRect* crop)
{
    GstFlowReturn ret = GST_FLOW_OK;
    switch (mOclVppType) {
    case IMG_PROC_TYPE_OCL_BLENDER:
        ret = process_image_blend(inbuf, inbuf2, crop);
        break;
    case IMG_PROC_TYPE_OCL_CROP:
        ret = process_image_crop(inbuf, outbuf, crop);
	break;
    default:
        ret = GST_FLOW_ERROR;
        break;
    }

    return ret;
}

GstFlowReturn ImageProcessor::process_image(GstBuffer* inbuf, std::shared_ptr<cv::UMat> outMat, VideoRect* crop)
{
    GstMfxSurface* surface = NULL;
    GstMfxVideoMeta* mfxMeta = NULL;
    GstMfxDisplay* mfxDisplay = NULL;
    VideoDisplayID display;
    VASurfaceID surface_id;

    //GST_MFX_DISPLAY_LOCK (mfxDisplay);
    mfxMeta = gst_buffer_get_mfx_video_meta(inbuf);
    surface = gst_mfx_video_meta_get_surface(mfxMeta);
    surface_id = (VASurfaceID)(gst_mfx_surface_get_id(surface));
    mfxDisplay = gst_mfx_surface_vaapi_get_display(surface);
    display = gst_mfx_display_get_vadisplay(mfxDisplay);
    gst_mfx_display_unref(mfxDisplay);

    /* input data must be NV12 surface from mfxdec element */
    mSrcFrame->fourcc = video_format_to_va_fourcc(GST_VIDEO_INFO_FORMAT(&mInVideoInfo));
    mSrcFrame->surface = surface_id;
    mSrcFrame->width = mInVideoInfo.width;
    mSrcFrame->height = mInVideoInfo.height;
    mSrcFrame->crop.x = crop->x;
    mSrcFrame->crop.y = crop->y;
    mSrcFrame->crop.height = crop->height;
    mSrcFrame->crop.width = crop->width;

    if (mSrcFrame->surface == INVALID_SURFACE_ID) {
        GST_ERROR("Failed to get VASurface!");
        return GST_FLOW_ERROR;
    }

    setup_ocl_context(display);

    if (mSrcFrame->fourcc != OCL_FOURCC_NV12) {
        GST_ERROR("only support RGBA blending on nv12 video frame");
        return GST_FLOW_ERROR;
    }

    mDstFrame->fourcc = OCL_FOURCC_RGB3;
    mDstFrame->mem = (cl_mem)outMat->handle(cv::ACCESS_RW);
    mDstFrame->width = crop->width;
    mDstFrame->height = crop->height;

    ocl_lock();
    OclStatus status = mOclVpp->process(mSrcFrame, mDstFrame);
    ocl_unlock();

    if (status == OCL_SUCCESS) {
        return GST_FLOW_OK;
    } else {
        return GST_FLOW_ERROR;
    }

    return GST_FLOW_OK;
}
