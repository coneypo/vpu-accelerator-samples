# Copyright (C) 2019 Intel Corporation
# SPDX-License-Identifier: MIT

FIND_PACKAGE(PkgConfig)
PKG_CHECK_MODULES(PC_CAIRO cairo) # FIXME: After we require CMake 2.8.2 we can pass QUIET to this call.

FIND_PATH(CAIRO_INCLUDE_DIRS
    NAMES cairo.h
    HINTS ${PC_CAIRO_INCLUDEDIR}
          ${PC_CAIRO_INCLUDE_DIRS}
    PATH_SUFFIXES cairo
)

FIND_LIBRARY(CAIRO_LIBRARIES
    NAMES cairo
    HINTS ${PC_CAIRO_LIBDIR}
          ${PC_CAIRO_LIBRARY_DIRS}
)

IF (CAIRO_INCLUDE_DIRS)
    IF (EXISTS "${CAIRO_INCLUDE_DIRS}/cairo-version.h")
        FILE(READ "${CAIRO_INCLUDE_DIRS}/cairo-version.h" CAIRO_VERSION_CONTENT)

        STRING(REGEX MATCH "#define +CAIRO_VERSION_MAJOR +([0-9]+)" _dummy "${CAIRO_VERSION_CONTENT}")
        SET(CAIRO_VERSION_MAJOR "${CMAKE_MATCH_1}")

        STRING(REGEX MATCH "#define +CAIRO_VERSION_MINOR +([0-9]+)" _dummy "${CAIRO_VERSION_CONTENT}")
        SET(CAIRO_VERSION_MINOR "${CMAKE_MATCH_1}")

        STRING(REGEX MATCH "#define +CAIRO_VERSION_MICRO +([0-9]+)" _dummy "${CAIRO_VERSION_CONTENT}")
        SET(CAIRO_VERSION_MICRO "${CMAKE_MATCH_1}")

        SET(CAIRO_VERSION "${CAIRO_VERSION_MAJOR}.${CAIRO_VERSION_MINOR}.${CAIRO_VERSION_MICRO}")
    ENDIF ()
ENDIF ()

# FIXME: Should not be needed anymore once we start depending on CMake 2.8.3
SET(VERSION_OK TRUE)
IF (Cairo_FIND_VERSION)
    IF (Cairo_FIND_VERSION_EXACT)
        IF ("${Cairo_FIND_VERSION}" VERSION_EQUAL "${CAIRO_VERSION}")
            # FIXME: Use IF (NOT ...) with CMake 2.8.2+ to get rid of the ELSE block
        ELSE ()
            SET(VERSION_OK FALSE)
        ENDIF ()
    ELSE ()
        IF ("${Cairo_FIND_VERSION}" VERSION_GREATER "${CAIRO_VERSION}")
            SET(VERSION_OK FALSE)
        ENDIF ()
    ENDIF ()
ENDIF ()

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Cairo DEFAULT_MSG CAIRO_INCLUDE_DIRS CAIRO_LIBRARIES VERSION_OK)
