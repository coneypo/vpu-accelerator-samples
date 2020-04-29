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
#include "utils/blockingqueue.h"

#include <chrono>
#include <gstmfxsurface_vaapi.h>
#include <iomanip>
#include <memory>
#include <opencv2/imgproc.hpp>
#include <string>

#define INTERVAL_IN_MS 1000

#ifdef __cplusplus
extern "C" {
#endif
static void putText(cv::Mat& mat, guint32 x, guint32 y, const std::string& text);
static void putRectangle(cv::Mat& mat, guint32 x, guint32 y, guint32 h, guint32 w);
static gboolean needCrop(gboolean isCropEnabled);
static void cropFrame(cv::Mat& mat, guint32 x, guint32 y, guint32 h, guint32 w);
static void drawLabelAndBoundingBox(cv::Mat& mat, guint32 x, guint32 y, guint32 h, guint32 w, const std::string& label, gdouble probability);

void blend(GstBuffer* buffer, BoundingBox* boxList, gint size, gboolean enableCrop)
{
    GstMfxVideoMeta* mfxMeta = gst_buffer_get_mfx_video_meta(buffer);
    GstMfxSurface* surface = gst_mfx_video_meta_get_surface(mfxMeta);
    gboolean cropFlag;
    if (!gst_mfx_surface_has_video_memory(surface)) {
        guchar* data = gst_mfx_surface_get_plane(surface, 0);
        guint frame_width = gst_mfx_surface_get_width(surface);
        guint frame_height = gst_mfx_surface_get_height(surface);
        cv::Mat mat(frame_height, frame_width, CV_8UC4, data);

        cropFlag = needCrop(enableCrop);

        for (int i = 0; i < size; i++) {
            if (cropFlag) {
                cropFrame(mat, boxList[i].x, boxList[i].y, boxList[i].height, boxList[i].width);
            }
            drawLabelAndBoundingBox(mat, boxList[i].x, boxList[i].y, boxList[i].height, boxList[i].width, boxList[i].label, boxList[i].probability);
        }
    } else {
        GST_ERROR("Frame is reside in video memory, please correct caps in your pipeline!");
    }
}

static void putText(cv::Mat& mat, guint32 x, guint32 y, const std::string& text)
{
    cv::putText(mat, text, cv::Point(x, y + 30), 1, 1.8, cv::Scalar(0, 255, 0, 255), 2);
}

static void putRectangle(cv::Mat& mat, guint32 x, guint32 y, guint32 h, guint32 w)
{
    cv::Rect rect(x, y, w, h);
    cv::rectangle(mat, rect, cv::Scalar(0, 255, 0, 255), 2);
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
    cv::Rect rect(x, y, w, h);
    std::shared_ptr<cv::Mat> croppedFrame = std::make_shared<cv::Mat>();
    cv::cvtColor(mat(rect), *croppedFrame, cv::COLOR_BGRA2RGB);
    BlockingQueue<std::shared_ptr<cv::Mat>>::instance().put(croppedFrame);
}

static void drawLabelAndBoundingBox(cv::Mat& mat, guint32 x, guint32 y, guint32 h, guint32 w, const std::string& label, gdouble probability)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3) << probability;
    std::string probStr = ss.str();

    // Write label and probility
    if ((w < (gint32)mat.cols / 10) || (h < (gint32)mat.rows / 10) || (w / (1.0 + h) > 3.0) || (h / (1.0 + w) > 3.0)) {
        putText(mat, x, y + 15, label + std::string("[") + probStr + std::string("]"));
    } else {
        putText(mat, x, y + 15, label);
        putText(mat, x, y + 45, std::string("prob=") + probStr);
    }
    // Draw rectangle on target object
    putRectangle(mat, x, y, h, w);
}

#ifdef __cplusplus
}
#endif
