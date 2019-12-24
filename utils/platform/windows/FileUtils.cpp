//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include <shlwapi.h>
#include <stdlib.h>
#include <string>
#include <windows.h>

#include "utils/FileUtils.h"
#include "utils/Mutex.h"

using namespace HddlUnite;

#pragma comment(lib, "shlwapi.lib")

namespace FileUtils {

std::string getDirectoryOfExecuteFile()
{
    TCHAR szPath[MAX_PATH];

    if (!GetModuleFileName(NULL, szPath, MAX_PATH)) {
        return std::string();
    }

    PathRemoveFileSpec(szPath);

    return szPath;
}

bool changeFileDesOwner(int fd, const char* user, const char* group)
{
    //gid_t gid = -1;
    //uid_t uid = -1;

    //if (fd <= 0) {
    //    errno = EINVAL;
    //    return false;
    //}

    //if (user != NULL) {
    //    // TODO:
    //}

    //if (group != NULL) {
    //    struct group* g = NULL;
    //    g = getgrnam(group);
    //    if (g == NULL) {
    //        HError("Error: Cannot get group id of group: %s\n", group);
    //        return false;
    //    }
    //    gid = g->gr_gid;

    //}

    //if (fchown(fd, uid, gid) < 0) {
    //    return false;
    //}

    return true;
}

bool changeFileOwner(const char* file, const char* user, const char* group)
{
    //gid_t gid = -1;
    //uid_t uid = -1;

    //if (file == NULL) {
    //    errno = EINVAL;
    //    return false;
    //}

    //if (!exist(file)) {
    //    HError("Error: file %s doesn't exist.", file);
    //    errno = EINVAL;
    //    return false;
    //}

    //if (user != NULL) {
    //    // TODO:
    //}

    //if (group != NULL) {
    //    struct group* g = NULL;
    //    g = getgrnam(group);
    //    if (g == NULL) {
    //        HError("Error: Cannot get group id of group: %s\n", group);
    //        return false;
    //    }
    //    gid = g->gr_gid;
    //}

    //if (chown(file, uid, gid) < 0) {
    //    return false;
    //}

    return true;
}

bool changeFileDesMode(int fd, int mode)
{
    //if (fd <= 0) {
    //    errno = EINVAL;
    //    return false;
    //}

    //if (mode >= 0) {
    //    if (fchmod(fd, mode) < 0) {
    //        return false;
    //    }
    //}

    return true;
}

std::string getHome()
{
    return getEnv("HOMEPATH");
}

bool liftMaxOpenFileLimit()
{
    return true;
}
} // End of namespace
