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
    unsigned videoWidth;        //Video Width
    unsigned videoHeight;       //Video height
    std::size_t fdActualLength; //Fd size
    float decFps;               //Decode FPS
#ifdef VALIDATION_DUMP
    ms frameStart;              //Start frame
    ms frameEnd;                //End frame
#endif
    bool drop;                  //Dropped or not
};

struct ImageMeta{
    unsigned imageWidth;                                                  //Image Width
    unsigned imageHeight;                                                 //Image Height
    std::chrono::time_point<std::chrono::steady_clock> pipeTimeStart;     //Image Blob start clock
#ifdef VALIDATION_DUMP
    ms frameStart;                                                        //Start Image iter count
    ms frameEnd;                                                          //End Image iter count
#endif
    bool drop;                                                            //Dropped or not
    std::string ImgName;                                                  //Image name
    unsigned WorkloadCount;                                               //Workload Count
    unsigned WorkloadStreamNum;                                           //Workload streams number
};

struct ROI {
    int32_t x {0};                          //Left of ROI
    int32_t y {0};                          //Top of ROI
    int32_t width {0};                      //Width of ROI
    int32_t height {0};                     //Height of ROI
    std::string labelClassification;        //Classification label
    int32_t labelIdClassification {0};      //Classification label id
    double confidenceClassification {0.0};  //Classfication confidence
    
    std::string labelDetection;             //Detection label
    int32_t labelIdDetection {0};           //Detection label id
    double confidenceDetection {0.0};       //Detection confidence

    std::size_t pts {0};                    //Pts
    uint64_t frameId {0};                   //Frame id
    uint64_t streamId {0};                  //Stream id

    //for tracking
    uint64_t trackingId {0};                //Tracking id
    HvaPipeline::TrackingStatus trackingStatus {HvaPipeline::TrackingStatus::LOST}; //Tracking status, default to lost so jpeg won't encode by default
};

struct InferMeta{
    int totalROI;           //Total ROI number
    int frameId;            //Frame id
    std::vector<ROI> rois;  //ROI vector
    float inferFps;         //Inference FPS
};

#endif //#ifndef COMMON_HPP
