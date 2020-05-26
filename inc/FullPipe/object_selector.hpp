#ifndef OBJECT_SELECTOR_HPP
#define OBJECT_SELECTOR_HPP

#include <memory>
#include <vector>
#include <map>
#include <tuple>
#include <string>
#include <vas/ot.h>
#include <opencv2/opencv.hpp>

class Object
{
public:
    Object(){};
    Object(int32_t id, const cv::Rect& r, uint64_t tid, int32_t cid, const std::string& label, double confidence)
        : oid(id), rect(r), tracking_id(tid), class_id(cid), class_label(label), confidence_classification(confidence)
    {}

public:
    int32_t     oid;
    cv::Rect    rect;
    uint64_t    tracking_id;
    int32_t     class_id;
    std::string class_label;
    double      confidence_classification;
};

using Objects = std::vector<Object>;

class ObjectSelector
{
public:
    ObjectSelector();
    explicit ObjectSelector(int32_t update_period);

public:
    ObjectSelector(const ObjectSelector&) = delete;
    ObjectSelector(ObjectSelector&&) = delete;
    ObjectSelector& operator=(const ObjectSelector&) = delete;
    ObjectSelector& operator=(ObjectSelector&&) = delete;

    virtual ~ObjectSelector();

public:
    int32_t getUpdatePeriod();
    void setUpdatePeriod(int32_t n);

public:
    std::tuple<Objects, Objects> preprocess(const Objects& objects);
    Objects postprocess(const Objects& classified, const Objects& tracked);

private:
    struct TrackInfo
    {
        uint64_t    tracking_id;  // tracking Id
        int32_t     class_id;     // class label
        std::string class_label;
        double      confidence_classification;
        int32_t     age;          // NEW: 1, Tracked: increased by 1, LOST: first, set 0 and decreased by 1.
    };

private:
    int32_t update_period_;
    std::map<uint64_t, TrackInfo> track_info_map_;
    std::map<uint64_t, TrackInfo> postproc_track_info_map_;

public:
    using Ptr = std::shared_ptr<ObjectSelector>;
};

#endif  // OBJECT_SELECTOR_HPP
