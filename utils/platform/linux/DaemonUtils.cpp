//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include <sys/wait.h>
#include <unistd.h>

#include "utils/DaemonUtils.h"
#include "utils/HLog.h"

using namespace HddlUnite;

namespace DaemonUtils {
bool createDaemon(std::vector<std::string> argv)
{
    if (argv.empty()) {
        HError("Error: empty argument vector as input");
        return false;
    }

    if (access(argv[0].c_str(), X_OK) < 0) {
        HError("Error: Daemon file %s has no execute permission.", argv[0]);
        return false;
    }

    std::stringstream ss;

    for (const auto& arg : argv) {
        ss << arg << ' ';
    }

    ss << '&';

    std::string command = ss.str();

    HDebug("system(%s)", command);

    int ret = system(command.c_str());

    return (WIFEXITED(ret) && (WEXITSTATUS(ret) == EXIT_SUCCESS));
}
} // End of namespace DaemonHelper
