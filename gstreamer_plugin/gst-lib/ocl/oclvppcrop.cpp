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

#include "oclvppcrop.h"
#include "Vppfactory.h"
#include "common.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <CL/cl.h>
#include <opencv2/core/ocl.hpp>
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace cv::ocl;

namespace HDDLStreamFilter {

OclStatus OclVppCrop::blend_helper()
{
    gboolean ret;
    m_kernel.args(m_src->cl_memory[0], m_src->cl_memory[1], m_dst->cl_memory[0],
        m_crop_x, m_crop_y, m_crop_w, m_crop_h);

    size_t globalWorkSize[2], localWorkSize[2];
    localWorkSize[0] = 8;
    localWorkSize[1] = 8;

    //globalWorkSize[0] = ALIGN_POW2 (m_src_w, 2 * localWorkSize[0]) / 2;
    //globalWorkSize[1] = ALIGN_POW2 (m_src_h, 2 * localWorkSize[1]) / 2;

    globalWorkSize[0] = ALIGN_POW2(m_crop_w, 2 * localWorkSize[0]) / 2;
    globalWorkSize[1] = ALIGN_POW2(m_crop_h, 2 * localWorkSize[1]) / 2;

    ret = m_kernel.run(2, globalWorkSize, localWorkSize, true);
    if (!ret) {
        GST_ERROR("%s() - failed to run kernel!!!\n", __func__);
        return OCL_FAIL;
    }

    return OCL_SUCCESS;
}

OclStatus OclVppCrop::process(const SharedPtr<VideoFrame>& src, const SharedPtr<VideoFrame>& dst)
{
    OclStatus status;

    m_src_w = src->width;
    m_src_h = src->height;

    m_crop_x = src->crop.x;
    m_crop_y = src->crop.y;
    m_crop_w = src->crop.width;
    m_crop_h = src->crop.height;

    if (!m_src_w || !m_src_h) {
        GST_ERROR("OclVppBlender failed due to invalid resolution\n");
        return OCL_INVALID_PARAM;
    }

    if (m_kernel.empty()) {
        GST_ERROR("OclVppBlender: invalid kernel\n");
        return OCL_FAIL;
    }

    m_src = (OclCLMemInfo*)m_context->acquireVAMemoryCL((VideoSurfaceID*)&src->surface, 2, CL_MEM_READ_ONLY);
    if (!m_src) {
        GST_ERROR("failed to acquire src va memory\n");
        m_context->releaseVAMemoryCL(&m_src);
        return OCL_FAIL;
    }

    m_dst = (OclCLMemInfo*)m_context->acquireMemoryCL(dst->mem, 1);
    if (!m_dst) {
        GST_ERROR("failed to acquire dst memory\n");
        m_context->finish();
        return OCL_FAIL;
    }

    status = blend_helper();
    m_context->releaseVAMemoryCL(&m_src);
    m_context->releaseMemoryCL(&m_dst);
    m_context->finish();
    return status;
}

const bool OclVppCrop::s_registered = OclVppFactory::register_<OclVppCrop>(OCL_VPP_MYCONVERTOR);

}
