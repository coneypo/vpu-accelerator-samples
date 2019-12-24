//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_MONITOR_H
#define HDDLUNITE_MONITOR_H

#include <functional>
#include <iostream>
#include <string>

#include "utils/Singleton.h"

namespace HddlUnite {
class FileMonitor : public Singleton<FileMonitor> {
public:
    enum class Event {
        Empty,
        Access,
        Modify,
        Open,
        CloseWrite,
        CloseNoWrite,
        DeleteSelf,
        All
    };
    using Callback = std::function<void(const std::string&, Event)>;

    FileMonitor();
    ~FileMonitor() override;

    bool addWatch(const std::string& filePath, Event event, const Callback& callback) noexcept;
    void deleteWatch(const std::string& filepath) noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
}

#endif //HDDLUNITE_MONITOR_H
