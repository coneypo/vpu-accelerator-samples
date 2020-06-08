#ifndef __TINY_YOLOV2__
#define __TINY_YOLOV2__
#include <cmath>
#include <vector>
#include <functional>

#include <inference_engine.hpp>
#include "detection_helper.hpp"

namespace YoloV2Tiny {
    /**
     * @brief Extract detection output from last layer feature map
     * @param pIn Input data pointer
     * @param anchor_idx Anchor index
     * @param cell_idx Cell index
     * @param threshold Confidence threshold
     * @param pOut Output data pointer
     */
    void fillRawNetOut(float const *pIn, const int anchor_idx, const int cell_ind, const float threshold,
                    float *pOut);
    using rawNetOutExtractor =
        std::function<void(float const *, const int, const int, const float threshold, float *)>;

    /**
     * @brief Yolo v2 region layer
     * @param blob Last layer blob
     * @param image_height Input image height
     * @param image_width Input image width
     * @param thresholdConf Confidence threshold
     * @param extractor Extractor for last layer feature map
     */
    std::vector<DetectedObject_t> TensorToBBoxYoloV2TinyCommon(const InferenceEngine::Blob::Ptr &blob, int image_height, int image_width,
                                     double thresholdConf, rawNetOutExtractor extractor);

} // namespace YoloV2Tiny

#endif //#ifndef __TINY_YOLOV2__
