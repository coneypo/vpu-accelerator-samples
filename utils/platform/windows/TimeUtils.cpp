//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include "utils/TimeUtils.h"

namespace TimeUtils {
bool localtimeSafe(const time_t* time, struct tm* result)
{
    if (time && result) {
        localtime_s(result, time);
        return true;
    }

    return false;
}
}
