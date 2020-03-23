#ifndef COMMON_HPP
#define COMMON_HPP

#include <cstdint> 
#include <string>
#include <vector>

using ms = std::chrono::milliseconds;

struct VideoMeta{
    unsigned videoWidth;
    unsigned videoHeight;
    std::size_t fdActualLength;
    float decFps;
};

struct ROI{
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    std::string label;
    std::size_t pts;
    double confidence;
    int indexROI;
};

struct InferMeta{
    int totalROI;
    int frameId;
    std::vector<ROI> rois;
    bool drop;
    float inferFps;
};

#endif //#ifndef COMMON_HPP