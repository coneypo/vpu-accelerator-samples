//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_DAEMONUTILS_H
#define HDDLUNITE_DAEMONUTILS_H

#include <string>
#include <vector>

namespace DaemonUtils {
bool createDaemon(std::vector<std::string> argv);
}
#endif //HDDLUNITE_DAEMONUTILS_H
