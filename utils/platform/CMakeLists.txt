#
#Copyright (C) 2019 Intel Corporation
#
#SPDX-License-Identifier: MIT
#
if(MSVC)
    add_subdirectory(windows)
else()
    add_subdirectory(linux)
endif()