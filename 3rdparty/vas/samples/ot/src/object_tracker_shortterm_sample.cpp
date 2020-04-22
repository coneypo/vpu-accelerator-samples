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

#include "vas/ot.h"
#include "vas/pvd.h"
#include <gflags/gflags.h>
#include <iostream>
#include <opencv2/opencv.hpp>


static void ExecuteShortTermTracking(const std::string& model_path, cv::VideoCapture* capture);


///
/// Input Argument Definitions.
///
const char* kUsageMessage =
    "\nThis application shows how to use object tracker from video frames.\n"
    "It shows object tracking results to the OpenCV window.\n"
    "shortterm_type_tracker [OPTION]\n"
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

int main(int argc, char** argv)
{
    gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);
    if (FLAGS_h || FLAGS_v.empty() || FLAGS_m.empty())
    {
        ShowUsage();
        exit(0);
    }

    cv::VideoCapture capture;
    if (FLAGS_v.size() == 1)
    {
        capture.open(atoi(FLAGS_v.c_str()));
    }
    else
    {
        capture.open(FLAGS_v);
    }

    if (!capture.isOpened())
    {
        std::cerr << "fail to get source frame!" << std::endl;
        return 1;
    }

    // For an external detector
    ExecuteShortTermTracking(FLAGS_m, &capture);

    return 0;
}

// Categorizing detected object's type
static int32_t GetClassLabel(vas::pvd::PersonVehicleType input_type)
{
    switch (input_type)
    {
    // Handle pedestrians
    case vas::pvd::PersonVehicleType::PERSON:
        return 0;

    // Handle 2-wheel vehicle types
    case vas::pvd::PersonVehicleType::BICYCLE:
    case vas::pvd::PersonVehicleType::MOTORBIKE:
        return 1;

    // Handle 4-wheel vehicle types
    case vas::pvd::PersonVehicleType::CAR:
    case vas::pvd::PersonVehicleType::TRUCK:
    case vas::pvd::PersonVehicleType::BUS:
    case vas::pvd::PersonVehicleType::VAN:
        return 2;

    // Others
    default:
        return 3;
    }
}

static void ExecuteShortTermTracking(const std::string& model_path, cv::VideoCapture* capture)
{
    static const int32_t kNumFramesToGetPeriodicalDetection = 30;  // User input

    try
    {
        // OT needs an external detector to get feed information of an tracking target. This sample uses the VAS PersonVehicleDetector.
        vas::pvd::PersonVehicleDetector::Builder    pvd_builder;
        auto pvd = pvd_builder.Build(model_path.c_str());

        vas::ot::ObjectTracker::Builder             ot_builder;
        auto ot = ot_builder.Build(vas::ot::TrackingType::SHORT_TERM);

        const char* window_name = "Object Tracker";
        cv::namedWindow(window_name, cv::WINDOW_NORMAL | cv::WINDOW_AUTOSIZE);

        int32_t index = 0;
        cv::Mat frame;

        while (capture->read(frame))
        {
            std::vector<vas::ot::DetectedObject> detected_objects;

            // Process to detect objects by PVD detector
            if (index % kNumFramesToGetPeriodicalDetection == 0)
            {
                // Conduct detection every frame
                auto objects = pvd->Detect(frame);
                for (const auto& object : objects)
                {
                    detected_objects.emplace_back(object.rect, GetClassLabel(object.type));
                }
            }

            // Conduct tracking
            auto tracked_objects = ot->Track(frame, detected_objects);
            for (const auto& object : tracked_objects)
            {
                std::string id = std::to_string(static_cast<int32_t>(object.tracking_id));
                id.append("(Class:" + std::to_string(object.class_label) + ")");
                if (object.status != vas::ot::TrackingStatus::LOST)
                {
                    cv::rectangle(frame, object.rect, cv::Scalar(0, 255, 0));
                    cv::putText(frame, id, cv::Point(object.rect.x - 1, object.rect.y - 1), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cv::Scalar(0, 0, 255), 1);
                }
                else
                {
                    // cv::rectangle(frame, object.rect, cv::Scalar(0, 0, 255));
                }
            }

            // Additionally draw the periodical input
            for (const auto& object : detected_objects)
                cv::rectangle(frame, object.rect, cv::Scalar(255, 0, 0), 1);

            cv::imshow(window_name, frame);

            int8_t key = cv::waitKey(1);
            if (key == 'q' || key == 'Q')
            {
                break;
            }

            ++index;
        }
    }
    catch (const std::exception& ex)
    {
        std::cout << ex.what() << std::endl;
    }

    cv::destroyAllWindows();
}
