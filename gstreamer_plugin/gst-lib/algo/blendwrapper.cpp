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

#include "blendwrapper.h"
#include "blockingqueue.h"
#include "imageproc.h"
#include <gstmfxdisplay.h>
#include <gstmfxsurface.h>
#include <gstmfxsurface_vaapi.h>
#include <gstmfxsurfacecomposition.h>
#include <memory>
#include <ocl/oclmemory.h>
#include <ocl/oclpool.h>
#include <opencv2/core/va_intel.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <va/va.h>

#define CHECK_VASTATUS(_status, _func)                                                                                 \
    if (_status != VA_STATUS_SUCCESS) {                                                                                \
        char str[256];                                                                                                 \
        snprintf(str, sizeof(str) - 1, "%s:%s (%d) failed(status=0x%08x),exit\n", __func__, _func, __LINE__, _status); \
        throw std::runtime_error(str);                                                                                 \
    }

#define INTERVAL_IN_MS 1000

#ifdef __cplusplus
extern "C" {
#endif

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
        img_processor->init(caps, IMG_PROC_TYPE_OCL_INPLACE_BLENDER);
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

    uint32_t x, y;
    cv::rectangle(mdraw, cv::Rect(0, 0, osd_mem->width, osd_mem->height), cv::Scalar(0), cv::FILLED);

    for (int i = 0; i < size; i++) {
        VideoRect rect;
        rect.x = boxList[i].x;
        rect.y = boxList[i].y;
        rect.height = boxList[i].height;
        rect.width = boxList[i].width;

        std::string strTxt;
        // Create an output string stream
        std::ostringstream stream_prob;
        stream_prob << std::fixed << std::setprecision(3) << boxList[i].probability;

        x = rect.x;
        y = rect.y + 30;

        // check if a small object
        if (((int)rect.width < osd_mem->width / 10) || ((int)rect.height < osd_mem->height / 10) || (rect.width / (1.0 + rect.height) > 3.0) || (rect.height / (1.0 + rect.width) > 3.0)) {
            // Write label and probility
            strTxt = std::string(boxList[i].label) + std::string("[") + stream_prob.str() + std::string("]");
            cv::putText(mdraw, strTxt, cv::Point(rect.x, rect.y - 15), 1, 1.8, cv::Scalar(255), 2); //Gray
        } else {
            // Write label and probility
            strTxt = std::string(boxList[i].label);
            cv::putText(mdraw, strTxt, cv::Point(x, y), 1, 1.8, cv::Scalar(255), 2); //Gray
            strTxt = std::string("prob=") + stream_prob.str();
            cv::putText(mdraw, strTxt, cv::Point(x, y + 30), 1, 1.8, cv::Scalar(255), 2);
        }

        // Draw rectangle on target object
        cv::Rect target_rect(rect.x, rect.y, rect.width, rect.height);
        cv::rectangle(mdraw, target_rect, cv::Scalar(255), 2);
    }
    return;
}

void cvdlhandler_process_osd(FrameHandler handle, GstBuffer* buffer, GstBuffer* osd_buf)
{
    CvdlHandler* cvdl_blender = (CvdlHandler*)handle;
    VideoRect rect = { 0, 0, (unsigned int)cvdl_blender->mImageWidth,
        (unsigned int)cvdl_blender->mImageHeight };

    ImageProcessor* img_processor = static_cast<ImageProcessor*>(cvdl_blender->mImgProcessor);
    img_processor->process_image(buffer, osd_buf, NULL, &rect);
}

void cvdlhandler_crop_frame(FrameHandler handle, GstBuffer* buffer, BoundingBox* box, guint32 num)
{
    static std::chrono::system_clock::time_point lastTimeStamp = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    if (INTERVAL_IN_MS < std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTimeStamp).count()) {
        lastTimeStamp = now;
        CvdlHandler* cvdl_blender = (CvdlHandler*)handle;
        for (guint32 i = 0; i < num; i++) {
            VideoRect rect = { (uint32_t)box[i].x, (uint32_t)box[i].y, (uint32_t)box[i].width, (uint32_t)box[i].height };
            std::shared_ptr<cv::UMat> croppedFrame = std::make_shared<cv::UMat>(cv::Size(rect.width, rect.height), CV_8UC3);
            ImageProcessor* img_processor = static_cast<ImageProcessor*>(cvdl_blender->mImgProcessor);
            img_processor->process_image(buffer, croppedFrame, &rect);

            if (BlockingQueue<std::shared_ptr<cv::UMat>>::instance().size() > 3) {
                BlockingQueue<std::shared_ptr<cv::UMat>>::instance().take();
            }
            BlockingQueue<std::shared_ptr<cv::UMat>>::instance().put(croppedFrame);
        }
    }
}

void cvdlhandler_process_boundingbox(FrameHandler handle, GstBuffer* buffer, BoundingBox* box, guint32 num)
{
    CvdlHandler* cvdl_blender = (CvdlHandler*)handle;
    ImageProcessor* img_processor = static_cast<ImageProcessor*>(cvdl_blender->mImgProcessor);
    img_processor->process_image(buffer, box, num);
}

#ifdef __cplusplus
}
#endif
