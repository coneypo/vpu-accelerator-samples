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


#include "oclvppblender.h"
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

OclStatus OclVppBlender::blend_helper()
{
    gboolean ret;
    m_kernel.args(m_src->cl_memory[0], m_src->cl_memory[1], m_src2->cl_memory[0], m_dst->cl_memory[0], m_dst->cl_memory[1], m_src_w, m_src_h);

    size_t globalWorkSize[2], localWorkSize[2];
    localWorkSize[0] = 16;
    localWorkSize[1] = 16;

    globalWorkSize[0] = ALIGN_POW2(m_src_w, 2 * localWorkSize[0]) / 2;
    globalWorkSize[1] = ALIGN_POW2(m_src_h, 2 * localWorkSize[1]) / 2;

    ret = m_kernel.run(2, globalWorkSize, localWorkSize, true);
    if (!ret) {
        GST_ERROR("%s() - failed to run kernel!!!\n", __func__);
        return OCL_FAIL;
    }
    return OCL_SUCCESS;
}

OclStatus OclVppBlender::process(const SharedPtr<VideoFrame>& src, const SharedPtr<VideoFrame>& osd, const SharedPtr<VideoFrame>& dst)
{
    OclStatus status;

    m_src_w = src->width;
    m_src_h = src->height;

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
        GST_ERROR("failed to acquire src2 va memory\n");
        m_context->releaseMemoryCL(&m_src);
        return OCL_FAIL;
    }
    m_src2 = (OclCLMemInfo*)m_context->acquireMemoryCL(osd->mem, 1);
    if (!m_src2) {
        GST_ERROR("failed to acquire src memory\n");
        m_context->finish();
        return OCL_FAIL;
    }

    m_dst = (OclCLMemInfo*)m_context->acquireVAMemoryCL((VideoSurfaceID*)&dst->surface, 2, CL_MEM_WRITE_ONLY);
    if (!m_dst) {
        GST_ERROR("failed to acquire src2 va memory\n");
        m_context->releaseMemoryCL(&m_dst);
        return OCL_FAIL;
    }

    status = blend_helper();

    m_context->releaseVAMemoryCL(&m_src);
    m_context->releaseMemoryCL(&m_src2);
    m_context->releaseVAMemoryCL(&m_dst);
    m_context->finish();
    return status;
}

const bool OclVppBlender::s_registered = OclVppFactory::register_<OclVppBlender>(OCL_VPP_MYBLENDER);
}
