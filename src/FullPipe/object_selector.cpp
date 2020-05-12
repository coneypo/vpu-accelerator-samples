#include "object_selector.hpp"
#include <limits>
#include <set>

#define MIN_NUM_LOST -30

ObjectSelector::ObjectSelector()
    : update_period_(std::numeric_limits<int32_t>::max())
{

}

ObjectSelector::ObjectSelector(int32_t update_period)
    : update_period_(update_period)
{
    if (update_period_ < 1)
    {
        throw std::runtime_error("update period must be bigger than 1.");
    }
}

ObjectSelector::~ObjectSelector()
{

}

int32_t ObjectSelector::getUpdatePeriod()
{
    return update_period_;
}

void ObjectSelector::setUpdatePeriod(int32_t n)
{
    update_period_ = n;
}

std::tuple<Objects, Objects> ObjectSelector::preprocess(const Objects& objects)
{
    Objects new_objs;
    Objects tracked_objs;
    std::set<uint64_t> id_set;

    // find new objects
    for (auto& o: objects)
    {
       auto tid = o.tracking_id;
       id_set.insert(tid);

       if (track_info_map_.find(tid) != track_info_map_.end())
       {
           auto& track_info = track_info_map_[tid];

           if (track_info.age + 1 > update_period_)
               new_objs.push_back(o);
           else  // set the class label of object from track info
               tracked_objs.push_back(o);
       }
       else
       {
           track_info_map_[tid] = {tid, -1, "unknown", 0.0, 1};
           new_objs.push_back(o);
       }
    }

    // remove the TrackInfo if its tracking id is not in id_set.
    for(auto it = track_info_map_.begin(); it != track_info_map_.end();)
    {
        if (id_set.find(it->first) == id_set.end())
        {
            it->second.age = std::min(it->second.age - 1, -1);
            if (it->second.age < MIN_NUM_LOST)
            {
                it = track_info_map_.erase(it);
            }
        }
        else
        {
            ++it;
        }
    }

    return std::make_tuple(new_objs, tracked_objs);
}

Objects ObjectSelector::postprocess(const Objects& classified, const Objects& tracked)
{
    // update the tracking table and merge two object vectors
    Objects result;
    std::set<uint64_t> id_set;

    for(auto& o : classified)
    {
        auto tid = o.tracking_id;
        id_set.insert(tid);
        postproc_track_info_map_[tid] = {o.tracking_id, o.class_id, o.class_label, o.confidence_classification, 1};
        result.push_back(o);
    }

    for(auto o : tracked)
    {
        auto tid = o.tracking_id;
        id_set.insert(tid);
        assert(postproc_track_info_map_.find(tid) != postproc_track_info_map_.end());

        // Increase the age of track info
        auto& track_info = postproc_track_info_map_[tid];
        o.class_id = track_info.class_id;
        o.class_label = track_info.class_label;
        o.confidence_classification = track_info.confidence_classification;

        track_info.age = std::max(track_info.age + 1, 1);
        result.push_back(o);
    }

    // remove the TrackInfo if its tracking id is not in id_set.
    for(auto it = postproc_track_info_map_.begin(); it != postproc_track_info_map_.end();)
    {
        if (id_set.find(it->first) == id_set.end())
        {
            it->second.age = std::min(it->second.age - 1, -1);
            if (it->second.age < MIN_NUM_LOST)
            {
                it = postproc_track_info_map_.erase(it);
            }
        }
        else
        {
            ++it;
        }
    }
    return result;
}
