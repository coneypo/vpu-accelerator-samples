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
#include <cmath>
#include <gflags/gflags.h>
#include <iostream>
#include <opencv2/opencv.hpp>

// #define WRITE_VIDEO_OUTPUT

typedef struct object_count_t
{
    int32_t num_ppl;
    int32_t num_2wheel;
    int32_t num_4wheel;
    int32_t num_bus;
}object_count_t;

typedef struct info_t
{
    bool            is_set;
    int32_t         line_idx;
    cv::Point       start;
    cv::Point       end;
    cv::Point2f     vec_ab;
    cv::Point2f     vec_ba;
    cv::Point2f     arrow_start;
    cv::Point2f     arrow_end;
    object_count_t  result;
} info_t;

typedef struct line_flow_t
{
    int32_t               tracking_id;
    int32_t               age;
    std::list<cv::Point>  ct_points;    // To draw a line of flow of each object

    // first : an index of a matching line('info_t')
    std::vector<std::pair<int32_t, float>>    prev_z;                 // second: (BA x BC).Z of a previous frame
    std::vector<std::pair<int32_t, float>>    crossing_direction;     // second: Direction of crossing a line
} line_flow_t;


static void ExecuteShortTermTracking(const std::string& model_path, cv::VideoCapture* capture);


///
/// Input Argument Definitions.
///
const char* kUsageMessage =
    "\nThis application shows how to use object tracker from video frames for the use case of 'counting pedestrian & car'.\n"
    "The VAS PVD component is used as an external detector to feed detection inputs to the object tracker\n"
    "It shows object tracking results to the OpenCV(version 4.0) window.\n"
    "shortterm_object_counting [OPTION]\n"
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

/*
 * Categorizing detected object's type
 * @param
 *  input_type      [in]    a type from the output of the PVD component
 */
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
    case vas::pvd::PersonVehicleType::VAN:
        return 2;

    // Bus
    case vas::pvd::PersonVehicleType::BUS:
        return 3;

    // Others
    default:
        return 4;
    }
}

static void IncreasCount(object_count_t* count, int32_t input_type)
{
    switch (input_type)
    {
    case 0:  // Handle pedestrians
        count->num_ppl++;
        return;
    case 1:  // Handle 2-wheel vehicle types
        count->num_2wheel++;
        return;
    case 2:  // Handle 4-wheel vehicle types
        count->num_4wheel++;
        return;
    case 3:  // Bus
    default:
        count->num_bus++;
        return;
    }
}

/*
 * @param
 *  input_image    [out] draw results on an input image
 *  input_type     [in]  categorized class label values from the GetClassLabel function
 *  input_rect     [in]  a rectangular coordinate to draw
 */
static void DrawRectByClassLabel(cv::Mat* input_image, int32_t input_type, cv::Rect input_rect)
{
    auto& frame = *input_image;
    switch (input_type)
    {
    case 0:  // Handle pedestrians
        cv::rectangle(frame, input_rect & cv::Rect(0, 0, frame.cols, frame.rows), cv::Scalar(255, 255, 0));
        return;
    case 1:  // Handle 2-wheel vehicle types
        cv::rectangle(frame, input_rect & cv::Rect(0, 0, frame.cols, frame.rows), cv::Scalar(0, 255, 255));
        return;
    case 2:  // Handle 4-wheel vehicle types
        cv::rectangle(frame, input_rect & cv::Rect(0, 0, frame.cols, frame.rows), cv::Scalar(255, 0, 255));
        return;
    case 3:  // Bus
    default:
        cv::rectangle(frame, input_rect & cv::Rect(0, 0, frame.cols, frame.rows), cv::Scalar(35, 96, 200));
        return;
    }

    return;
}

/*
 * Draw lines of flow of tracking objects using center points
 * @param
 *  input_image    [out] draw results on an input image
 *  input_type     [in]  categorized class label values from the GetClassLabel function
 *  ct_points      [in]  a list of center points
 */
static void DrawLineByFlow(cv::Mat* input_image, int32_t input_type, const std::list<cv::Point>& ct_points)
{
    auto& frame = *input_image;
    for (const auto& point : ct_points)
    {
        switch (input_type)
        {
        case 0:  // Handle pedestrians
            cv::circle(frame, point, 2, cv::Scalar(255, 255, 0));
            continue;
        case 1:  // Handle 2-wheel vehicle types
            cv::circle(frame, point, 2, cv::Scalar(0, 255, 255));
            continue;
        case 2:  // Handle 4-wheel vehicle types
            cv::circle(frame, point, 2, cv::Scalar(255, 0, 255));
            continue;
        case 3:  // Bus
        default:
            cv::circle(frame, point, 2, cv::Scalar(35, 96, 200));
            continue;
        }
    }

    return;
}

/*
 * Write results
 * @param
 *  input_image         [out] draw results on an input image
 *  cross_count         [in]  the number of counted objects
 *  location            [in]  coordinates for writing result counts of each line
 */
static void DrawResultText(cv::Mat* input_image, object_count_t cross_count, const cv::Point& location)
{
    auto& frame = *input_image;
    cv::Point result_loc;
    result_loc.x = location.x;
    result_loc.y = location.y + 80;

    cv::putText(frame, "Person:" + std::to_string(cross_count.num_ppl),     cv::Point(result_loc.x, result_loc.y - 60), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.0, cv::Scalar(255, 255, 0), 1);
    cv::putText(frame, "2-Wheel:" + std::to_string(cross_count.num_2wheel), cv::Point(result_loc.x, result_loc.y - 40), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.0, cv::Scalar(0, 255, 255), 1);
    cv::putText(frame, "4-Wheel:" + std::to_string(cross_count.num_4wheel), cv::Point(result_loc.x, result_loc.y - 20), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.0, cv::Scalar(255, 0, 255), 1);
    cv::putText(frame, "Bus&etc:" + std::to_string(cross_count.num_bus),    cv::Point(result_loc.x, result_loc.y),      cv::FONT_HERSHEY_COMPLEX_SMALL, 1.0, cv::Scalar(35, 96, 200), 1);
    return;
}

/*
 * Draw a line and fill the struct 'info_t'
 */
static void DrawLine(int32_t event, int32_t  x, int32_t  y, int32_t  flag, void *param)
{
    std::vector<info_t>* info_vec = static_cast<std::vector<info_t>*>(param);

    int32_t              curr_max_index = 0;
    for (const auto& line : *info_vec)
    {
        if (line.line_idx >= curr_max_index)
            curr_max_index = line.line_idx + 1;
    }

    if (event == cv::EVENT_LBUTTONDOWN)
    {
        info_t info;

        info.is_set = false;
        info.line_idx = curr_max_index;
        info.start.x = x;
        info.start.y = y;
        info.result = { 0, };

        info_vec->emplace_back(info);
    }
    else if (event == cv::EVENT_MOUSEMOVE)
    {
        // Move
    }
    else if (event == cv::EVENT_LBUTTONUP)
    {
        auto info = std::find_if(info_vec->begin(), info_vec->end(), [](info_t line)
        {
            return line.is_set == false;
        });

        if (info == info_vec->end())
            return;

        info->is_set = true;
        info->end.x = x;
        info->end.y = y;
        info->vec_ab = cv::Point2f((info->end.x - info->start.x), (info->end.y - info->start.y));
        info->vec_ba = cv::Point2f((info->start.x - info->end.x), (info->start.y - info->end.y));

        info->arrow_start = cv::Point2f(static_cast<float>(info->start.x) + info->vec_ab.x / 2, static_cast<float>(info->start.y) + info->vec_ab.y / 2);
        float mag = sqrtf(info->vec_ab.x * info->vec_ab.x + info->vec_ab.y * info->vec_ab.y);
        float size = mag / 4;
        info->vec_ab = info->vec_ab / mag;
        info->vec_ba = info->vec_ba / mag;

        info->arrow_end = cv::Point2f(info->arrow_start.x - info->vec_ab.y * size, info->arrow_start.y + info->vec_ab.x * size);
    }

    return;
}

/*
 * Check a tracking object is crossing the drawn line
 * @param
 *  param               [in]  information of a drawn line by a use
 *  ct_point            [in]  a center point of a tracking object at a current frame
 *  prev_z              [in]  a result of cross product at a previous frame to know which direction a object was located
 *  curr_z              [out] a result of cross product at a current frame
 */
static bool CheckCross(const info_t& param, cv::Point ct_point, float prev_z, float* curr_z)
{
    // A: start point, B: end point
    {
        cv::Point2f vec_ac(ct_point.x - param.start.x, ct_point.y - param.start.y);
        cv::Point2f vec_bc(ct_point.x - param.end.x, ct_point.y - param.end.y);

        float dot_a = param.vec_ab.x * vec_ac.x + param.vec_ab.y * vec_ac.y;
        float dot_b = param.vec_ba.x * vec_bc.x + param.vec_ba.y * vec_bc.y;

        float cro_prod = param.vec_ba.x * vec_bc.y - param.vec_ba.y * vec_bc.x;
        *curr_z = cro_prod;

        if (dot_a > 0.f && dot_b > 0.f)
        {
            if (prev_z > 0.f && (*curr_z) <= 0.f)  // Count if it moves from left to right
            {
                // Cross
                return true;
            }
        }
    }

    return false;
}

// #define WRITE_VIDEO_OUTPUT

static void ExecuteShortTermTracking(const std::string& model_path, cv::VideoCapture* capture)
{
    // const int32_t   kNumObjectType = sizeof(object_count_t) / sizeof(object_count_t::num_ppl);
    const int32_t   kMaxNumLineFlow = 60;
    const int32_t   kOutdatedObject = 90;

    // User input
    const int32_t   kNumFramesToGetPeriodicalDetection = 10;  // This sample code run the PVD detector and feed its detection info to the tracker every 5 frames.
    const int32_t   kNumTrackingTarget = 40;                 // The number of maximum tracking target. A default value is 30.

    int32_t         wait_key = 1;

    std::vector<info_t>      target_lines;
    std::vector<line_flow_t> line_flow;

    cv::Mat frame;
    capture->read(frame);

#ifdef WRITE_VIDEO_OUTPUT
    std::string output_dir = PATH_OUTPUT;
    cv::VideoWriter video(output_dir + "/ot_dump.avi", cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 30, cv::Size(frame.cols, frame.rows));
#endif

    std::vector<object_count_t> total_cross_line;

    try
    {
        // Tracker needs an external detector feeding information of detected objects. This sample uses the latest version of the VAS PersonVehicleDetector.
        vas::pvd::PersonVehicleDetector::Builder    pvd_builder;
        auto pvd = pvd_builder.Build(model_path.c_str());

        // Setting of an object tracker
        vas::ot::ObjectTracker::Builder             ot_builder;
        ot_builder.max_num_objects = kNumTrackingTarget;
        auto ot = ot_builder.Build(vas::ot::TrackingType::SHORT_TERM);

        const char* window_name = "Object Tracker";
        cv::namedWindow(window_name, cv::WINDOW_NORMAL | cv::IMREAD_COLOR);
        cv::setMouseCallback(window_name, DrawLine, &target_lines);

        int32_t index = 0;

        do
        {
            std::vector<vas::ot::DetectedObject> detected_objects;

            // Process to detect objects by PVD detector
            if (index % kNumFramesToGetPeriodicalDetection == 0)
            {
                auto objects = pvd->Detect(frame);

                for (const auto& object : objects)
                    detected_objects.emplace_back(object.rect, GetClassLabel(object.type));
            }

            // Conduct tracking using the inputs from PVD
            auto tracked_objects = ot->Track(frame, detected_objects);

            for (auto& line : target_lines)
            {
                if (line.is_set == false)
                {
                    line.result = { 0, };
                }
            }

            // Post processing with the tracking result for display
            for (const auto& object : tracked_objects)
            {
                // To use a center point as a line of flow
                cv::Point ct_point(std::max(0, object.rect.x + (object.rect.width / 2)), std::max(0, object.rect.y + (object.rect.height / 2)));

                // Match linse_flow_t with tracking results to know how each object flows
                int is_found = false;
                for (auto iter = line_flow.begin(); iter != line_flow.end(); ++iter)
                {
                    if (iter->tracking_id == object.tracking_id)
                    {
                        is_found = true;
                        if (iter->ct_points.size() >= kMaxNumLineFlow)
                        {
                            iter->ct_points.pop_front();
                        }

                        iter->age = 0;
                        iter->ct_points.emplace_back(ct_point);

                        for (auto& line : target_lines)
                        {
                            float curr_z = 0.f;
                            auto matching_prev  = std::find_if(iter->prev_z.begin(), iter->prev_z.end(), [&line](std::pair<int32_t, float> prev_z)
                            {
                                return prev_z.first == line.line_idx;
                            });
                            auto matching_direc = std::find_if(iter->crossing_direction.begin(), iter->crossing_direction.end(), [&line](std::pair<int32_t, float> crossing_direction)
                            {
                                return crossing_direction.first == line.line_idx;
                            });

                            if (line.is_set == true)  // If a user drew a line
                            {
                                if (matching_prev == iter->prev_z.end())
                                {
                                    // The target line has been newly drawn. Add new vector for the new drawn line
                                    CheckCross(line, ct_point, 0.f, &curr_z);
                                    iter->prev_z.emplace_back(line.line_idx, curr_z);
                                    iter->crossing_direction.emplace_back(line.line_idx, 0.f);
                                }
                                else
                                {
                                    if (CheckCross(line, ct_point, matching_prev->second, &curr_z) == true && matching_direc->second == 0.f)
                                    {
                                        IncreasCount(&line.result, object.class_label);
                                        matching_direc->second = curr_z;
                                    }
                                    matching_prev->second = curr_z;
                                }
                            }
                            else
                            {
                                //
                            }
                        }

                        DrawLineByFlow(&frame, object.class_label, iter->ct_points);
                    }
                }  // Each line, 'line_flow_t'

                // Add new one
                if (is_found == false)
                {
                    line_flow_t new_line_flow;
                    new_line_flow.tracking_id = object.tracking_id;
                    new_line_flow.age = 0;
                    new_line_flow.ct_points.emplace_back(ct_point);

                    for (const auto& line : target_lines)
                    {
                        float curr_z = 0.f;
                        CheckCross(line, ct_point, 0, &curr_z);

                        new_line_flow.prev_z.emplace_back(line.line_idx, curr_z);
                        new_line_flow.crossing_direction.emplace_back(line.line_idx, 0.f);
                    }

                    line_flow.emplace_back(new_line_flow);
                }

                // Draw rectangles of all tracking objects
                if (object.status != vas::ot::TrackingStatus::LOST)
                {
                    std::string id = std::to_string(static_cast<int32_t>(object.tracking_id));

                    DrawRectByClassLabel(&frame, object.class_label, object.rect);
                    cv::putText(frame, id, cv::Point(object.rect.x - 1, object.rect.y - 1), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cv::Scalar(0, 0, 255), 1);
                }
                else
                {
                    // Lost
                }
            }  // Each 'tracked_objects'

            // Draw all the lines and information
            cv::putText(frame, std::to_string(index), cv::Point(1, 10), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cv::Scalar(255, 255, 255), 1);  // number of a frame
            for (const auto& line : target_lines)
            {
                if (line.is_set == true)
                {
                    cv::line(frame, line.start, line.end, cv::Scalar(0, 0, 255), 3);
                    cv::arrowedLine(frame, cv::Point(line.arrow_start), cv::Point(line.arrow_end), cv::Scalar(0, 0, 255), 1, 4);
                    DrawResultText(&frame, line.result, line.end);
                }
            }

            // Remove a target which is not being tracked anymore
            for (auto iter = line_flow.begin(); iter != line_flow.end();)
            {
                bool is_being_tracked = false;
                for (const auto& object : tracked_objects)
                {
                    if (iter->tracking_id == object.tracking_id)
                    {
                        is_being_tracked = true;
                    }
                }

                if (is_being_tracked == false)
                {
                    iter = line_flow.erase(iter);
                    continue;
                }

                if (iter->age >= kOutdatedObject)
                {
                    iter = line_flow.erase(iter);
                    continue;
                }
                else
                {
                    iter->age += 1;
                    ++iter;
                    continue;
                }
            }

#if 0
            // Draw the periodical inputs for the PVD detector
            for (const auto& object : detected_objects)
                cv::rectangle(frame, object.rect, cv::Scalar(255, 0, 0), 2);
#endif

            cv::imshow(window_name, frame);

#ifdef WRITE_VIDEO_OUTPUT
            video.write(frame);
#endif
            int8_t key = cv::waitKey(wait_key);
            if (key == 'q' || key == 'Q')
            {
                break;
            }
            else if (key == 's' || key == 'S')
            {
                wait_key = 0;
            }
            else if (key == 'd' || key == 'D')
            {
                target_lines.clear();
                for (auto& line : line_flow)
                {
                    line.prev_z.clear();
                    line.crossing_direction.clear();
                }
            }

            ++index;
        } while (capture->read(frame));
    }
    catch (const std::exception& ex)
    {
        std::cout << ex.what() << std::endl;
    }

#ifdef WRITE_VIDEO_OUTPUT
    video.release();
#endif

    line_flow.clear();
    cv::destroyAllWindows();
}
