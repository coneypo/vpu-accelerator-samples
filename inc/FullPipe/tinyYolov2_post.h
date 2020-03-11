#ifndef __TINY_YOLOV2__
#define __TINY_YOLOV2__
#include <cmath>
#include <vector>
#include <functional>
#include <inference_engine.hpp>
#include "detection_helper.hpp"

namespace YoloV2Tiny {
    std::vector<DetectedObject_t> run_nms(std::vector<DetectedObject_t> candidates, double threshold);

    static void scaleBack(bool keep_ratio, float &xmin, float &xmax, float &ymin, float &ymax, int image_width,
                        int image_height);

    constexpr int num_classes = 20;
    enum RawNetOut {
        idxX = 0,
        idxY = 1,
        idxW = 2,
        idxH = 3,
        idxScale = 4,
        idxClassProbBegin = 5,
        idxClassProbEnd = idxClassProbBegin + num_classes,
        idxCount = idxClassProbEnd
    };

    // void fillRawNetOutMoviTL(float const *pIn, const int anchor_idx, const int cell_ind, const float threshold,
    //                         float *pOut);


    void fillRawNetOut(float const *pIn, const int anchor_idx, const int cell_ind, const float threshold,
                    float *pOut);
    using rawNetOutExtractor =
        std::function<void(float const *, const int, const int, const float threshold, float *)>;

    std::vector<DetectedObject_t> TensorToBBoxYoloV2TinyCommon(const InferenceEngine::Blob::Ptr &blob, int image_height, int image_width,
                                     double thresholdConf, rawNetOutExtractor extractor);

} // namespace YoloV2Tiny

#endif //#ifndef __TINY_YOLOV2__
