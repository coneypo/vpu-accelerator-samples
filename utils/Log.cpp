//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include "utils/Log.h"

namespace HddlUnite {
uint32_t Log::defaultLogLevel = static_cast<uint32_t>(LogLevel::INFO) | static_cast<uint32_t>(LogLevel::WARN) | static_cast<uint32_t>(LogLevel::ERROR) | static_cast<uint32_t>(LogLevel::FATAL);
}