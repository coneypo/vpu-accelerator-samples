#ifndef OBJECT_TRACKING_NODE_HPP
#define OBJECT_TRACKING_NODE_HPP

#include <string>
#include <hvaPipeline.hpp>
#include "common.hpp"
#include "HddlUnite.h"

//to be replaced by real tracker
namespace FakeOT
{

enum class TrackingType
{
    LONG_TERM,
    SHORT_TERM,
    ZERO_TERM
};


enum class TrackingStatus
{
    NEW,         /**< The object is newly added. */
    TRACKED,     /**< The object is being tracked. */
    LOST         /**< The object gets lost now. The object can be tracked again automatically(long term tracking) or by specifying detected object manually(short term and zero term tracking). */
};


class DetectedObject
{
public:
    uint16_t top {0};
    uint16_t left {0};
    uint16_t width {0};
    uint16_t height {0};
    int32_t class_label {0};
};


class Object
{
public:
    uint16_t top {0};
    uint16_t left {0};
    uint16_t width {0};
    uint16_t height {0};

    uint64_t tracking_id {0};
    int32_t class_label {0};
    TrackingStatus status {TrackingStatus::NEW};
};


class FakeTracker
{
public:
    FakeTracker(int32_t input_workloadId)
    :workloadId(input_workloadId)
    {

    };

    inline void setVideoBufferProperty(int32_t input_fd, int32_t input_heightImage, int32_t input_widthImage)
    {
        heightImage = input_heightImage;
        widthImage = input_widthImage;
        fdLastFrame = fdCurrentFrame;
        fdCurrentFrame = input_fd;
    };

    inline std::vector<Object> track(std::vector<DetectedObject> vecDetectedObjects)
    {
        std::vector<Object> vecObjectsLastFrame = vecObjects;

        vecObjects.clear();

        for (DetectedObject& detectedObject : vecDetectedObjects)
        {
            Object object;
            object.top = detectedObject.top;
            object.left = detectedObject.left;
            object.height = detectedObject.height;
            object.width = detectedObject.width;
            object.class_label = detectedObject.class_label;
            object.tracking_id = cntObject++;
            object.status = TrackingStatus::NEW;

            vecObjects.push_back(object);


            printf("[debug] object class label: %d, tracking id: %ld, object status: %d\n", 
                    object.class_label, object.tracking_id, (int)object.status);
        }
        return vecObjects;
    }

    uint64_t cntObject{0};

    int32_t workloadId{0};
    int32_t heightImage{0};
    int32_t widthImage{0};
    int32_t fdCurrentFrame{0};
    int32_t fdLastFrame{0};
    std::vector<Object> vecObjects;
};

} //namespace FakeOT


class ObjectTrackingNode : public hva::hvaNode_t{
public:
    ObjectTrackingNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, 
        std::vector<WorkloadID> vWorkloadId, int32_t reservedInt, std::string reservedStr);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    mutable int32_t m_cntNodeWorker{0};

    std::vector<WorkloadID> m_vWorkloadId;
    int32_t m_reservedInt{0};
    std::string m_reservedStr;
};

class ObjectTrackingNodeWorker : public hva::hvaNodeWorker_t{
public:
    ObjectTrackingNodeWorker(hva::hvaNode_t* parentNode,
        WorkloadID workloadId, int32_t reservedInt, std::string reservedStr);

    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

    WorkloadID m_workloadId{0ul};
    float m_fps{0.0f};
    float m_durationAve {0.0f};
    uint64_t m_cntFrame{0ul};


    FakeOT::FakeTracker m_fakeTracker;

    int32_t m_reservedInt{0};
    std::string m_reservedStr;

private:

    std::vector<std::shared_ptr<hva::hvaBlob_t>> m_vecBlobInput;
};

#endif //#ifndef OBJECT_TRACKING_NODE_HPP