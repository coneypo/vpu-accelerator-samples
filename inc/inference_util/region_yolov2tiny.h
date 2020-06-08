//
// Copyright 2019 Intel Corporation.
//
// This software and the related documents are Intel copyrighted materials,
// and your use of them is governed by the express license under which they
// were provided to you (End User License Agreement for the Intel(R) Software
// Development Products (Version May 2017)). Unless the License provides
// otherwise, you may not use, modify, copy, publish, distribute, disclose or
// transmit this software or the related documents without Intel's prior
// written permission.
//
// This software and the related documents are provided as is, with no
// express or implied warranties, other than those that are expressly
// stated in the License.
//

#pragma once

namespace postprocess
{
/**
 * @brief Yolo region layer
 * @param data Output data from inference
 * @param shape4D Last layer shape
 * @param strides4D Last layer stride
 * @param thresh Detection confidence threshold
 * @param nms NMS threshold
 * @param num_classes Detection class number
 * @param image_width Input image width
 * @param image_height Input image height
 * @param result Output detection result
 * @return Status
 */
int yolov2(const float *data, int * shape4D, int * strides4D, float thresh, float nms, 
        int num_classes,
        int image_width, int image_height,
        float * result);

};