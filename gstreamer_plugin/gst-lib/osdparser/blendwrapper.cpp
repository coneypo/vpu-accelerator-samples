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
static void putRectange(cv::Mat& mat, guint32 x, guint32 y, guint32 h, guint32 w);

void blend(GstBuffer* buffer, BoundingBox* boxList, gint size, gboolean enableCrop)
{
    GstMfxVideoMeta* mfxMeta = gst_buffer_get_mfx_video_meta(buffer);
    GstMfxSurface* surface = gst_mfx_video_meta_get_surface(mfxMeta);
    gboolean needCrop = FALSE;
    if (!gst_mfx_surface_has_video_memory(surface)) {
        guchar* data = gst_mfx_surface_get_plane(surface, 0);
        guint frame_width = gst_mfx_surface_get_width(surface);
        guint frame_height = gst_mfx_surface_get_height(surface);
        cv::Mat mat(frame_height, frame_width, CV_8UC4, data);

        if (enableCrop) {
            static auto lastTimeStamp = std::chrono::system_clock::now();
            auto now = std::chrono::system_clock::now();
            if (INTERVAL_IN_MS < std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTimeStamp).count()) {
                lastTimeStamp = now;
                needCrop = TRUE;
            } else {
                needCrop = FALSE;
            }
        }

        for (int i = 0; i < size; i++) {
            guint32 x = boxList[i].x;
            guint32 y = boxList[i].y;
            guint32 width = boxList[i].width;
            guint32 height = boxList[i].height;

            if (needCrop) {
                cv::Rect rect(x, y, width, height);
                std::shared_ptr<cv::Mat> croppedFrame = std::make_shared<cv::Mat>(cv::Size(boxList[i].width, boxList[i].height), CV_8UC3);
                cv::cvtColor(mat(rect), *croppedFrame, cv::COLOR_BGRA2RGB);
                BlockingQueue<std::shared_ptr<cv::Mat>>::instance().put(croppedFrame);
            }

            std::string text;
            std::ostringstream stream_prob;
            stream_prob << std::fixed << std::setprecision(3) << boxList[i].probability;

            // Write label and probility
            if ((width < frame_width / 10) || (height < frame_height / 10) || (width / (1.0 + height) > 3.0) || (height / (1.0 + width) > 3.0)) {
                text = std::string(boxList[i].label) + std::string("[") + stream_prob.str() + std::string("]");
                putText(mat, x, y + 15, text);
            } else {
                text = std::string(boxList[i].label);
                putText(mat, x, y + 15, text);
                text = std::string("prob=") + stream_prob.str();
                putText(mat, x, y + 45, text);
            }
            // Draw rectangle on target object
            putRectange(mat, x, y, width, height);
        }
    } else {
        GST_ERROR("Frame is reside in video memory, please correct caps in your pipeline!");
    }
}

static void putText(cv::Mat& mat, guint32 x, guint32 y, const std::string& text)
{
    cv::putText(mat, text, cv::Point(x, y + 30), 1, 1.8, cv::Scalar(0, 255, 0, 255), 2);
}

static void putRectange(cv::Mat& mat, guint32 x, guint32 y, guint32 h, guint32 w)
{
    cv::Rect rect(x, y, w, h);
    cv::rectangle(mat, rect, cv::Scalar(0, 255, 0, 255), 2);
}

#ifdef __cplusplus
}
#endif
