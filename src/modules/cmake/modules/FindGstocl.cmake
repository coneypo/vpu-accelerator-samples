# Copyright (C) 2019 Intel Corporation
# SPDX-License-Identifier: MIT

FIND_PATH(GSTOCL_INCLUDE_DIRS
    NAMES gstocl/oclcommon.h
    HINTS /usr/include/
          /usr/local/include/
    PATH_SUFFIXES json-c
)

FIND_LIBRARY(GSTOCL_LIBRARIES
    NAMES gstocl
    HINTS /usr/lib/
          /usr/local/lib/
          /usr/lib/gstreamer-1.0
          /usr/local/lib/gstreamer-1.0
)

SET(VERSION_OK TRUE)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Gstocl DEFAULT_MSG GSTOCL_INCLUDE_DIRS GSTOCL_LIBRARIES VERSION_OK)
