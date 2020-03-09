# Copyright (C) 2019 Intel Corporation
# SPDX-License-Identifier: MIT
#
# - Try to find HarfBuzz
# Once done, this will define
#
#  HarfBuzz_FOUND - system has HarfBuzz
#  HarfBuzz_INCLUDE_DIRS - the HarfBuzz include directories
#  HarfBuzz_LIBRARIES - link these to use HarfBuzz

find_package(PkgConfig)
pkg_check_modules(PC_HARFBUZZ QUIET harfbuzz)
set(LIBHARFBUZZ_DEFINITIONS ${PC_HARFBUZZ_CFLAGS_OTHER})
find_path(LIBHARFBUZZ_INCLUDE_DIR hb.h
	HINTS ${PC_HARFBUZZ_INCLUDEDIR} ${PC_HARFBUZZ_INCLUDE_DIRS}
	PATHS
		/usr/include/
		/usr/include/harfbuzz/
	)
find_library(LIBHARFBUZZ_LIBRARY NAMES harfbuzz
	HINTS ${PC_HARFBUZZ_LIBDIR} ${PC_HARFBUZZ_LIBRARY_DIRS})

set(LIBHARFBUZZ_LIBRARIES ${LIBHARFBUZZ_LIBRARY} )
set(LIBHARFBUZZ_INCLUDE_DIRS ${LIBHARFBUZZ_INCLUDE_DIR})
include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRES arguments and set LIBDBI_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(libharfbuzz DEFAULT_MSG
	LIBHARFBUZZ_LIBRARY LIBHARFBUZZ_INCLUDE_DIR)
mark_as_advanced(LIBHARFBUZZ_INCLUDE_DIR LIBHARFBUZZ_LIBRARY)
