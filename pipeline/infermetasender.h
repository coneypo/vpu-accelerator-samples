//
// Created by xiao on 2020/1/22.
//
#include "utils/IPC.h"
#include "utils/HLog.h"

#ifndef HDDLDEMO_INFERMETASENDER_H
#define HDDLDEMO_INFERMETASENDER_H

using namespace HddlUnite;

class InferMetaSender{
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
  }

  void serializeSave(int roi_x, int roi_y, int roi_width, int roi_height, const std::string& label = std::string(), size_t pts = 0, double probability = 0)
  {
    std::string single_element = std::to_string(roi_x) + "," + std::to_string(roi_y) + "," + std::to_string(roi_width) + "," + std::to_string(roi_height)
                                 + "," + label + "," + std::to_string(pts) + "," + std::to_string(probability) + ",";
    m_serialized_result += single_element;
  }

  bool send()
  {
    std::string to_send_data(std::move(m_serialized_result));
    int length = static_cast<int>(to_send_data.length());
    if (length <= 0) {
      HError("Error: invalid message length, length=%lu", length);
      return false;
    }
    AutoMutex autoLock(m_connection->getMutex());

    if (!m_connection->write(&length, sizeof(length))) {
      HError("Error: write message length failed");
      return false;
    }

    if (!m_connection->write(&to_send_data[0], length)) {
      HError("Error: write message failed, expectLen=%lu", length);
      return false;
    }
    return true;
  }
};
#endif // HDDLDEMO_INFERMETASENDER_H
