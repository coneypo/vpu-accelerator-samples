
FIND_PACKAGE(PkgConfig)
PKG_CHECK_MODULES(PC_JSON-C json-c) # FIXME: After we require CMake 2.8.2 we can pass QUIET to this call.

FIND_PATH(JSON-C_INCLUDE_DIRS
    NAMES json.h
    HINTS ${PC_JSON-C_INCLUDEDIR}
          ${PC_JSON-C_INCLUDE_DIRS}
    PATH_SUFFIXES json-c
)

FIND_LIBRARY(JSON-C_LIBRARIES
    NAMES json-c
    HINTS ${PC_JSON-C_LIBDIR}
          ${PC_JSON-C_LIBRARY_DIRS}
)

IF (JSON-C_INCLUDE_DIRS)
    IF (EXISTS "${JSON-C_INCLUDE_DIRS}/json-c-version.h")
        FILE(READ "${JSON-C_INCLUDE_DIRS}/json-c-version.h" JSON-C_VERSION_CONTENT)

        STRING(REGEX MATCH "#define +JSON-C_VERSION_MAJOR +([0-9]+)" _dummy "${JSON-C_VERSION_CONTENT}")
        SET(JSON-C_VERSION_MAJOR "${CMAKE_MATCH_1}")

        STRING(REGEX MATCH "#define +JSON-C_VERSION_MINOR +([0-9]+)" _dummy "${JSON-C_VERSION_CONTENT}")
        SET(JSON-C_VERSION_MINOR "${CMAKE_MATCH_1}")

        STRING(REGEX MATCH "#define +JSON-C_VERSION_MICRO +([0-9]+)" _dummy "${JSON-C_VERSION_CONTENT}")
        SET(JSON-C_VERSION_MICRO "${CMAKE_MATCH_1}")

        SET(JSON-C_VERSION "${JSON-C_VERSION_MAJOR}.${JSON-C_VERSION_MINOR}.${JSON-C_VERSION_MICRO}")
    ENDIF ()
ENDIF ()

# FIXME: Should not be needed anymore once we start depending on CMake 2.8.3
SET(VERSION_OK TRUE)
IF (Json-c_FIND_VERSION)
    IF (Json-c_FIND_VERSION_EXACT)
        IF ("${Json-c_FIND_VERSION}" VERSION_EQUAL "${JSON-C_VERSION}")
            # FIXME: Use IF (NOT ...) with CMake 2.8.2+ to get rid of the ELSE block
        ELSE ()
            SET(VERSION_OK FALSE)
        ENDIF ()
    ELSE ()
        IF ("${Json-c_FIND_VERSION}" VERSION_GREATER "${JSON-C_VERSION}")
            SET(VERSION_OK FALSE)
        ENDIF ()
    ENDIF ()
ENDIF ()

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Json-c DEFAULT_MSG JSON-C_INCLUDE_DIRS JSON-C_LIBRARIES VERSION_OK)
