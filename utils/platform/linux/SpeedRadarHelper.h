//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_SPEEDRADARHELPER_H
#define HDDLUNITE_SPEEDRADARHELPER_H

#include <csignal>
#include <functional>
#include <unistd.h>

#include "utils/HLog.h"
#include "utils/NonCopyable.h"

namespace HddlUnite {
class SpeedRadarHelper : public NonCopyable {
public:
    using Routinue = std::function<void()>;

    void setParentProcessRoutine(Routinue parentRoutine) noexcept
    {
        m_parentProcessRoutine = parentRoutine;
    }

    void setChildProcessRoutine(Routinue childRoutine) noexcept
    {
        m_childProcessRoutine = childRoutine;
    }

    void run() noexcept
    {
        auto pid = fork();
        if (pid < 0) {
            HError("Error: create child process failed.");
            return;
        }

        if (pid) {
            m_parentProcessRoutine();
            kill(pid, SIGKILL);
        } else {
            m_childProcessRoutine();
        }
    }

private:
    Routinue m_parentProcessRoutine;
    Routinue m_childProcessRoutine;
};
}

#endif //HDDLUNITE_SPEEDRADARHELPER_H
