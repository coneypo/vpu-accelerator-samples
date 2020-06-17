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


#include "blendwrapper.h"
#include "utils/blockingqueue.h"
#include "utils/cropdefs.h"

#include <chrono>
#include <gstmfxsurface_vaapi.h>
#include <iomanip>
#include <memory>
#include <opencv2/imgproc.hpp>
#include <string>

#ifdef ENABLE_INTEL_VA_OPENCL
#include "imageproc.h"
#include "oclpool.h"
#endif

#define INTERVAL_IN_MS 1000
#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_INTEL_VA_OPENCL
#define GREEN cv::Scalar(255)
#else
#define GREEN cv::Scalar(0, 255, 0, 255)
#endif

static void putText(cv::Mat& mat, guint32 x, guint32 y, const std::string& text);
static void putRectangle(cv::Mat& mat, guint32 x, guint32 y, guint32 h, guint32 w);
static gboolean needCrop(gboolean isCropEnabled);
static void cropFrame(cv::Mat& mat, guint32 x, guint32 y, guint32 h, guint32 w);
static void drawLabelAndBoundingBox(cv::Mat& mat, guint32 x, guint32 y, guint32 h, guint32 w, const std::string& label, gdouble probability);

void blend(GstOsdParser* filter, GstBuffer* buffer, BoundingBox* boxList, gint size, gboolean enableCrop)
{
    GstMfxVideoMeta* mfxMeta = gst_buffer_get_mfx_video_meta(buffer);
    GstMfxSurface* surface = gst_mfx_video_meta_get_surface(mfxMeta);
    if (!gst_mfx_surface_has_video_memory(surface)) {
        guchar* data = gst_mfx_surface_get_plane(surface, 0);
        guint frame_width = gst_mfx_surface_get_width(surface);
        guint frame_height = gst_mfx_surface_get_height(surface);
        cv::Mat mat(frame_height, frame_width, CV_8UC4, data);

        if (needCrop(enableCrop)) {
            for (int i = 0; i < size; i++) {
                cropFrame(mat, boxList[i].x, boxList[i].y, boxList[i].height, boxList[i].width);
            }
        }
        for (int i = 0; i < size; i++) {
            drawLabelAndBoundingBox(mat, boxList[i].x, boxList[i].y, boxList[i].height, boxList[i].width, boxList[i].label, boxList[i].probability);
        }

    } else {
#ifdef ENABLE_INTEL_VA_OPENCL
        if (needCrop(enableCrop)) {
            cvdlhandler_crop_frame(filter->crop_handle, buffer, boxList, size);
        }

        GstBuffer* osd_buf;
        cvdlhandler_generate_osd(filter->blend_handle, boxList, size, &osd_buf);
        cvdlhandler_process_osd(filter->blend_handle, buffer, osd_buf);
        gst_buffer_unref(osd_buf);
#else
        GST_ERROR("Frame is reside in video memory, please correct caps in your pipeline!");
#endif
    }
    (void)filter;
}

static void putText(cv::Mat& mat, guint32 x, guint32 y, const std::string& text)
{
    int baseline;
    cv::Size textSize = cv::getTextSize(text, 1, 1.8, 2, &baseline);
    if (x + textSize.width <= static_cast<guint32>(mat.cols) && y + textSize.height <= static_cast<guint32>(mat.rows)) {
        cv::putText(mat, text, cv::Point(x, y + 30), 1, 1.8, GREEN, 2);
    }
}

static void putRectangle(cv::Mat& mat, guint32 x, guint32 y, guint32 h, guint32 w)
{
    if (x + w <= static_cast<guint32>(mat.cols) && y + h <= static_cast<guint32>(mat.rows)) {
        cv::Rect rect(x, y, w, h);
        cv::rectangle(mat, rect, GREEN, 2);
    }
}

static gboolean needCrop(gboolean isCropEnabled)
{
    if (!isCropEnabled) {
        return FALSE;
    }
    static auto lastTimeStamp = std::chrono::system_clock::now();
    auto now = std::chrono::system_clock::now();
    if (INTERVAL_IN_MS < std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTimeStamp).count()) {
        lastTimeStamp = now;
        return TRUE;
    }
    return FALSE;
}

static void cropFrame(cv::Mat& mat, guint32 x, guint32 y, guint32 h, guint32 w)
{
    if (x + w <= static_cast<guint32>(mat.cols) && y + h <= static_cast<guint32>(mat.rows)) {
        cv::Rect rect(x, y, w, h);
        std::shared_ptr<cv::Mat> croppedFrame = std::make_shared<cv::Mat>();
        cv::Mat tempMat;
        cv::cvtColor(mat(rect), tempMat, cv::COLOR_BGRA2RGB);
        cv::resize(tempMat, *croppedFrame, cv::Size(CROP_IMAGE_WIDTH, CROP_IMAGE_HEIGHT));
        BlockingQueue<std::shared_ptr<cv::Mat>>::instance().put(croppedFrame);
    }
}

static void drawLabelAndBoundingBox(cv::Mat& mat, guint32 x, guint32 y, guint32 h, guint32 w, const std::string& label, gdouble probability)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3) << probability;
    std::string probStr = ss.str();

    // Write label and probility
    if ((w < (guint32)mat.cols / 10) || (h < (guint32)mat.rows / 10) || (w / (1.0 + h) > 3.0) || (h / (1.0 + w) > 3.0)) {
        putText(mat, x, y + 15, label + std::string("[") + probStr + std::string("]"));
    } else {
        putText(mat, x, y + 15, label);
        putText(mat, x, y + 45, std::string("prob=") + probStr);
    }
    // Draw rectangle on target object
    putRectangle(mat, x, y, h, w);
}

#ifdef ENABLE_INTEL_VA_OPENCL

FrameHandler cvdlhandler_create()
{
    CvdlHandler* cvdl_blender = (CvdlHandler*)g_new0(CvdlHandler, 1);
    ImageProcessor* img_processor = new ImageProcessor;
    cvdl_blender->mImgProcessor = static_cast<void*>(img_processor);
    cvdl_blender->mInited = false;

    return (FrameHandler)cvdl_blender;
}

void cvdlhandler_init(FrameHandler handle, GstCaps* caps, const char* ocl_format)
{
    CvdlHandler* cvdl_blender = (CvdlHandler*)handle;
    GstVideoInfo info;
    int width, height; //, size;
    GstCaps* ocl_caps;

    if (cvdl_blender->mInited)
        return;

    // Get pad caps for video width/height
    //caps = gst_pad_get_current_caps(pad);
    gst_video_info_from_caps(&info, caps);
    width = info.width;
    height = info.height;

    // ocl pool will allocate the same size osd buffer with format
    ocl_caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, ocl_format, NULL);
    gst_caps_set_simple(ocl_caps, "width", G_TYPE_INT, width, "height",
        G_TYPE_INT, height, NULL);

    gst_video_info_from_caps(&info, ocl_caps);
    cvdl_blender->mOsdPool = ocl_pool_create(ocl_caps, info.size, 4, 10);
    gst_caps_unref(ocl_caps);

    // init imgage processor: it will not allocate ocl buffer in it
    ImageProcessor* img_processor = static_cast<ImageProcessor*>(cvdl_blender->mImgProcessor);

    // here we only use gray image for doing inplace blender, andl "BRGA" for cropping image
    if (strcmp(ocl_format, "GRAY8") == 0) {
        img_processor->init(caps, IMG_PROC_TYPE_OCL_BLENDER);
    } else {
        img_processor->init(caps, IMG_PROC_TYPE_OCL_CROP);
    }
    img_processor->get_input_video_size(&cvdl_blender->mImageWidth,
        &cvdl_blender->mImageHeight);

    cvdl_blender->mInited = true;
    //gst_caps_unref(caps);
}

void cvdlhandler_destroy(FrameHandler handle)
{
    CvdlHandler* cvdl_blender = (CvdlHandler*)handle;
    ImageProcessor* img_processor = static_cast<ImageProcessor*>(cvdl_blender->mImgProcessor);

    delete img_processor;
    if (cvdl_blender->mOsdPool)
        gst_object_unref(cvdl_blender->mOsdPool);
    cvdl_blender->mOsdPool = NULL;
    cvdl_blender->mImgProcessor = NULL;
    free(cvdl_blender);
}

GstBuffer* cvdlhandler_get_free_buffer(FrameHandler handle)
{
    CvdlHandler* cvdl_blender = (CvdlHandler*)handle;
    GstBuffer* free_buf = NULL;
    OclMemory* free_mem = NULL;

    free_buf = ocl_buffer_alloc(cvdl_blender->mOsdPool);
    g_return_val_if_fail(free_buf, NULL);

    free_mem = ocl_memory_acquire(free_buf);
    if (!free_mem) {
        gst_buffer_unref(free_buf);
        return NULL;
    }

    free_mem->purpose = 1;
    return free_buf;
}

void cvdlhandler_generate_osd(FrameHandler handle, BoundingBox* boxList, gint size, GstBuffer** osd_buf)
{
    OclMemory* osd_mem = NULL;

    *osd_buf = cvdlhandler_get_free_buffer(handle);
    g_return_if_fail(*osd_buf != NULL);

    osd_mem = ocl_memory_acquire(*osd_buf);
    if (!osd_mem) {
        gst_buffer_unref(*osd_buf);
        return;
    }
    osd_mem->purpose = 2;

    cv::Mat mdraw = osd_mem->frame.getMat(cv::ACCESS_WRITE);
    cv::rectangle(mdraw, cv::Rect(0, 0, osd_mem->width, osd_mem->height), cv::Scalar(0), cv::FILLED);

    for (int i = 0; i < size; i++) {
        drawLabelAndBoundingBox(mdraw, boxList[i].x, boxList[i].y, boxList[i].height, boxList[i].width, boxList[i].label, boxList[i].probability);
    }
}

void cvdlhandler_process_osd(FrameHandler handle, GstBuffer* buffer, GstBuffer* osd_buf)
{
    CvdlHandler* cvdl_blender = (CvdlHandler*)handle;
    VideoRect rect = { 0, 0, (unsigned int)cvdl_blender->mImageWidth, (unsigned int)cvdl_blender->mImageHeight };

    ImageProcessor* img_processor = static_cast<ImageProcessor*>(cvdl_blender->mImgProcessor);
    img_processor->process_image(buffer, osd_buf, NULL, &rect);
}

void cvdlhandler_crop_frame(FrameHandler handle, GstBuffer* buffer, BoundingBox* box, guint32 num)
{
    for (guint32 i = 0; i < num; i++) {
        VideoRect rect = { (uint32_t)box[i].x, (uint32_t)box[i].y, (uint32_t)box[i].width, (uint32_t)box[i].height };
        std::shared_ptr<cv::UMat> croppedFrame = std::make_shared<cv::UMat>(cv::Size(rect.width, rect.height), CV_8UC3);
        ImageProcessor* img_processor = static_cast<ImageProcessor*>(handle->mImgProcessor);
        img_processor->process_image(buffer, croppedFrame, &rect);

        std::shared_ptr<cv::Mat> image = std::make_shared<cv::Mat>();
        cv::resize(*croppedFrame, *image, cv::Size(CROP_IMAGE_WIDTH, CROP_IMAGE_HEIGHT));
        BlockingQueue<std::shared_ptr<cv::Mat>>::instance().put(image);
    }
}

#endif
#ifdef __cplusplus
}
#endif
