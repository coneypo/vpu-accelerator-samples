//
// Created by xiao on 2020/1/22.
//

#ifndef HDDLDEMO_INFERMETASENDER_H
#define HDDLDEMO_INFERMETASENDER_H

#include "ipc.h"
#include <iostream>
#include <thread>

using namespace HddlUnite;

class InferMetaSender {
private:
    Connection::Ptr m_connection;
    Poller::Ptr m_poller;
    std::string m_serialized_result;

public:
    bool connectServer(const std::string& server_name)
    {
        m_poller = Poller::create();
        m_connection = Connection::create(m_poller);
        for (int i = 0; i < 10; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            if (m_connection->connect(server_name)) {
                std::cout << "succeed connecting" << server_name << std::endl;
                return true;
            }
        }
        return false;
    }

    void serializeSave(int roi_x, int roi_y, int roi_width, int roi_height, const std::string& label = std::string(),
            size_t pts = 0, double probability = 0, float inferFps = 0.0, float decFps = 0.0)
    {
        std::string single_element = std::to_string(roi_x) + "," + std::to_string(roi_y) + "," + std::to_string(roi_width) + "," + std::to_string(roi_height)
            + "," + label + "," + std::to_string(pts) + "," + std::to_string(probability) + ","+ std::to_string(inferFps) + ","+ std::to_string(decFps) + ",";
        m_serialized_result += single_element;
    }

    void ImgserializeSave(int roi_x, int roi_y, int roi_width, int roi_height, const std::string& label = std::string(), 
            size_t pts = 0, double probability = 0, float inferFps = 0.0, float decFps = 0.0, const std::string& imgName = std::string())
    {
        std::string single_element = std::to_string(roi_x) + "," + std::to_string(roi_y) + "," + std::to_string(roi_width) + "," + std::to_string(roi_height)
            + "," + label + "," + std::to_string(pts) + "," + std::to_string(probability) + ","+ std::to_string(inferFps) + ","+ std::to_string(decFps) + "," + imgName  + ",";
        m_serialized_result += single_element;
    }

    bool send()
    {
        std::string to_send_data(std::move(m_serialized_result));
        int length = static_cast<int>(to_send_data.length());
        if (length <= 0) {
            return false;
        }
        std::lock_guard<std::mutex> lock(m_connection->getMutex());

        if (!m_connection->write(&length, sizeof(length))) {
            return false;
        }

        if (!m_connection->write(&to_send_data[0], length)) {
            return false;
        }
        m_serialized_result.clear();
        return true;
    }
};

#endif // HDDLDEMO_INFERMETASENDER_H
