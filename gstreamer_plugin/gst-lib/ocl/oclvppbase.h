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

#ifndef _VPP_BASE_H_
#define _VPP_BASE_H_


#include <CL/opencl.h>
#include <opencv2/core/ocl.hpp>
#include <opencv2/opencv.hpp>

#include "boundingbox.h"
#include "oclcommon.h"
#include "vppinterface.h"

namespace HDDLStreamFilter {

class OclVppBase : public VppInterface {
public:
    OclVppBase() = default;
    virtual ~OclVppBase();

    virtual OclStatus process(const SharedPtr<VideoFrame>&, const SharedPtr<VideoFrame>&);
    virtual OclStatus process(const SharedPtr<VideoFrame>&, const SharedPtr<VideoFrame>&, const SharedPtr<VideoFrame>&);
    virtual OclStatus setOclContext(const SharedPtr<OclContext>&);
    virtual const char* getKernelFileName() { return nullptr; }
    virtual const char* getKernelName() { return nullptr; }
    OclStatus printOclKernelInfo();

protected:
    cv::ocl::Kernel m_kernel;
    SharedPtr<OclContext> m_context;
};

}

#endif /* _VPP_BASE_H_ */
