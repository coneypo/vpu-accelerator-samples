/*
 * INTEL CONFIDENTIAL
 * Copyright 2018-2019 Intel Corporation
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or
 * its suppliers and licensors. The Material contains trade secrets and
 * proprietary and confidential information of Intel or its suppliers and
 * licensors. The Material is protected by worldwide copyright and trade secret
 * laws and treaty provisions. No part of the Material may be used, copied,
 * reproduced, modified, published, uploaded, posted, transmitted, distributed,
 * or disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be express
 * and approved by Intel in writing.
 */

#include "vas/pvd.h"
#include "vas/ot.h"
#include <gflags/gflags.h>
#include <iostream>
#include <memory>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <atomic>
#include <opencv2/opencv.hpp>
#include <thread>


const double kTargetFps = 25.0;
const double kDelay     = 1.5;


///
/// Input Argument Definitions.
///
const char* kUsageMessage =
    "\nThis application shows how to use object tracker from video frames.\n"
    "It shows object tracking results to the OpenCV window.\n"
    "shortterm_type_tracker_mt [OPTION]\n"
    "Options:";

const char* kHelpMessage = "Show help messages";
const char* kVideoSourceMessage = "Required. Path to source video file or camera device ID. type: string";
const char* kPvdModelMessage = "Required. Path to directory of PersonVehicleDetector model files. type: string";

DEFINE_bool(h, false, kHelpMessage);
DEFINE_string(v, "", kVideoSourceMessage);
DEFINE_string(m, "", kPvdModelMessage);

void ShowUsage()
{
    std::cout << kUsageMessage << std::endl;
    std::cout << "\t-h\t\t" << kHelpMessage << std::endl;
    std::cout << "\t-v\t<path>\t" << kVideoSourceMessage << std::endl;
    std::cout << "\t-m\t<path>\t" << kPvdModelMessage << std::endl;
}

class FrameData
{
public:
    enum class Status
    {
        PVD_STARTED,
        PVD_FINISHED,
        NO_PVD,
        TERMINATING
    };

public:
    cv::Mat frame;
    std::vector<vas::pvd::PersonVehicle> person_vehicles;
    std::vector<vas::ot::Object> objects;
    std::atomic<Status> status;
};


class FrameDataQueue
{
public:
    void Enqueue(std::shared_ptr<FrameData> frame_data)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(frame_data);
        cond_.notify_one();
    }

    std::shared_ptr<FrameData> Dequeue()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this]() { return !queue_.empty(); });
        auto frame_data = queue_.front();
        queue_.pop();
        return frame_data;
    }

    size_t GetSize() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    std::queue<std::shared_ptr<FrameData>> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
};


class PvdWorker
{
public:
    explicit PvdWorker(const std::string model_path) : frame_data_(nullptr), working_(false)
    {
        vas::pvd::PersonVehicleDetector::Builder pvd_builder;
        pvd_ = pvd_builder.Build(model_path.c_str());
    }

    void Run()
    {
        while (true)
        {
            while (!working_)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            if (frame_data_->status == FrameData::Status::TERMINATING)
                break;

            frame_data_->person_vehicles = pvd_->Detect(frame_data_->frame);
            frame_data_->status = FrameData::Status::PVD_FINISHED;
            working_ = false;
        }
    }

    bool TrySetFrameData(std::shared_ptr<FrameData> frame_data)
    {
        if (working_ == true)
            return false;

        frame_data_ = frame_data;
        working_ = true;

        return true;
    }

private:
    std::unique_ptr<vas::pvd::PersonVehicleDetector> pvd_;
    std::shared_ptr<FrameData> frame_data_;
    std::atomic<bool> working_;
};


class OtWorker
{
public:
    OtWorker(std::shared_ptr<FrameDataQueue>& input_queue, std::shared_ptr<FrameDataQueue>& display_queue) : input_queue_(input_queue), display_queue_(display_queue)
    {
        vas::ot::ObjectTracker::Builder ot_builder;
        ot_ = ot_builder.Build(vas::ot::TrackingType::SHORT_TERM);
    }

    void Run()
    {
        while (true)
        {
            auto frame_data = input_queue_->Dequeue();
            if (frame_data->status == FrameData::Status::TERMINATING)
                break;

            if (frame_data->status == FrameData::Status::PVD_STARTED)
                while (frame_data->status != FrameData::Status::PVD_FINISHED)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

            std::vector<vas::ot::DetectedObject> detected_objects;

            if (frame_data->status == FrameData::Status::PVD_FINISHED)
                std::for_each(frame_data->person_vehicles.begin(), frame_data->person_vehicles.end(), [&detected_objects](const vas::pvd::PersonVehicle& pv) { detected_objects.emplace_back(pv.rect, static_cast<int32_t>(pv.type)); });

            frame_data->objects = ot_->Track(frame_data->frame, detected_objects);
            display_queue_->Enqueue(frame_data);
        }
    }

private:
    std::unique_ptr<vas::ot::ObjectTracker> ot_;
    std::shared_ptr<FrameDataQueue> input_queue_;
    std::shared_ptr<FrameDataQueue> display_queue_;
};


int main(int argc, char** argv)
{
    gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);
    if (FLAGS_h || FLAGS_v.empty() || FLAGS_m.empty())
    {
        ShowUsage();
        exit(0);
    }

    try
    {
        std::cout << "initializing..." << std::endl;

        auto input_queue  = std::make_shared<FrameDataQueue>();
        auto output_queue = std::make_shared<FrameDataQueue>();

        PvdWorker pvd_worker(FLAGS_m);
        OtWorker  ot_worker(input_queue, output_queue);

        std::thread pvd_thread([&pvd_worker]() { pvd_worker.Run(); });
        std::thread ot_thread([&ot_worker]() { ot_worker.Run(); });

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        cv::VideoCapture capture;
        if (FLAGS_v.size() == 1)
        {
            capture.open(atoi(FLAGS_v.c_str()));
        }
        else
        {
            capture.open(FLAGS_v);
        }

        bool display_started = false;
        const char* window_name = "OT+PVD";

        cv::namedWindow(window_name, cv::WINDOW_NORMAL | cv::WINDOW_AUTOSIZE);

        std::cout << "running..." << std::endl;

        while (true)
        {
            auto frame_start = std::chrono::high_resolution_clock::now();

            auto frame_data = std::make_shared<FrameData>();
            if (capture.read(frame_data->frame))
            {
                frame_data->status = FrameData::Status::PVD_STARTED;
                if (!pvd_worker.TrySetFrameData(frame_data))
                    frame_data->status = FrameData::Status::NO_PVD;

                input_queue->Enqueue(frame_data);

                if (!display_started && output_queue->GetSize() > static_cast<int32_t>(kDelay * kTargetFps))
                    display_started = true;

                if (!display_started)
                    continue;
            }
            else
            {
                if (output_queue->GetSize() == 0)
                    break;
            }

            auto output = output_queue->Dequeue();
            for (const auto& object : output->objects)
            {
                if (object.status != vas::ot::TrackingStatus::LOST)
                    cv::rectangle(output->frame, object.rect, cv::Scalar(0, 255, 0));
            }

            cv::imshow(window_name, output->frame);

            int8_t key = cv::waitKey(1);
            if (key == 'q' || key == 'Q')
                break;

            auto frame_end = std::chrono::high_resolution_clock::now();
            auto elapsed   = std::chrono::duration_cast<std::chrono::duration<double>>(frame_end - frame_start);
            auto sleep_amount = std::max(0.005, (1.0 / kTargetFps) - elapsed.count());
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int32_t>(sleep_amount * 1000)));
        }

        std::cout << "terminating..." << std::endl;

        cv::destroyAllWindows();

        auto frame_data = std::make_shared<FrameData>();
        frame_data->status = FrameData::Status::TERMINATING;

        input_queue->Enqueue(frame_data);
        while (!pvd_worker.TrySetFrameData(frame_data))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        ot_thread.join();
        pvd_thread.join();
    }
    catch (const std::exception& ex)
    {
        std::cout << ex.what() << std::endl;
        cv::destroyAllWindows();
        return -1;
    }

    return 0;
}
