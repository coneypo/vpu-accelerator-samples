#ifndef COMMON_HPP
#define COMMON_HPP

#include <cstdint> 
#include <string>
#include <vector>
#include <chrono>

#define VALIDATION_DUMP

using ms = std::chrono::milliseconds;

namespace HvaPipeline
{
enum class TrackingStatus
{
    NEW,         /**< The object is newly added. */
    TRACKED,     /**< The object is being tracked. */
    LOST         /**< The object gets lost now. The object can be tracked again automatically(long term tracking) or by specifying detected object manually(short term and zero term tracking). */
};


} //namespace HvaPipeline

struct VideoMeta{
    unsigned videoWidth;
    unsigned videoHeight;
    std::size_t fdActualLength;
    float decFps;
#ifdef VALIDATION_DUMP
    ms frameStart;
    ms frameEnd;
#endif
    bool drop;
};

struct ImageMeta{
    unsigned imageWidth;
    unsigned imageHeight;
    std::chrono::time_point<std::chrono::steady_clock> pipeTimeStart;
#ifdef VALIDATION_DUMP
    ms frameStart;
    ms frameEnd;
#endif
    bool drop;
    std::string ImgName;
};

struct ROI {
    int32_t x {0};
    int32_t y {0};
    int32_t width {0};
    int32_t height {0};
    std::string labelClassification;
    int32_t labelIdClassification {0};
    double confidenceClassification {0.0};
    
    std::string labelDetection;
    int32_t labelIdDetection {0};
    double confidenceDetection {0.0};

    std::size_t pts {0};
    uint64_t frameId {0};
    uint64_t streamId {0};

    //for tracking
    uint64_t trackingId {0};
    HvaPipeline::TrackingStatus trackingStatus {HvaPipeline::TrackingStatus::LOST}; //default to lost so jpeg won't encode by default
};

struct InferMeta{
    int totalROI;
    int frameId;
    std::vector<ROI> rois;
    // bool drop;
    float inferFps;

    float durationDetection{0.0f};
    float durationClassification{0.0f};

    // bool drop{false};
};

#endif //#ifndef COMMON_HPP
