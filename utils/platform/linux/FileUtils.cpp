//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include <climits>
#include <cstdlib>
#include <grp.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "utils/FileUtils.h"
#include "utils/HLog.h"

namespace FileUtils {
std::string getDirectoryOfExecuteFile()
{
    char filePath[512];

    if (readlink("/proc/self/exe", filePath, 512) <= 0) {
        return std::string();
    }

    auto pCh = strrchr(filePath, '/');
    if (!pCh) {
        return std::string();
    }

    *pCh = '\0';

    return filePath;
}

bool changeFileDesOwner(int fd, const char* user, const char* group)
{
    gid_t gid = -1;
    uid_t uid = -1;

    if (fd <= 0) {
        errno = EINVAL;
        return false;
    }

    if (user) {
        // TODO:
    }

    if (group) {
        struct group* g = getgrnam(group);
        if (!g) {
            HError("Error: Cannot get group id of group: %s\n", group);
            return false;
        }
        gid = g->gr_gid;
    }

    if (fchown(fd, uid, gid) < 0) {
        return false;
    }

    return true;
}

bool changeFileOwner(const char* file, const char* user, const char* group)
{
    gid_t gid = -1;
    uid_t uid = -1;

    if (!file) {
        errno = EINVAL;
        return false;
    }

    if (!exist(file)) {
        HError("Error: file %s doesn't exist.", file);
        errno = EINVAL;
        return false;
    }

    if (user) {
        // TODO:
    }

    if (group) {
        struct group* g = getgrnam(group);
        if (!g) {
            HError("Error: Cannot get group id of group: %s\n", group);
            return false;
        }
        gid = g->gr_gid;
    }

    if (chown(file, uid, gid) < 0) {
        return false;
    }

    return true;
}

bool changeFileDesMode(int fd, int mode)
{
    static_assert(sizeof(mode) >= sizeof(mode_t), "int is small than mode_t");
    if (fd <= 0) {
        errno = EINVAL;
        return false;
    }

    if (mode >= 0) {
        if (fchmod(fd, (mode_t)mode) < 0) {
            return false;
        }
    }

    return true;
}

std::string getHome()
{
    return getEnv("HOME");
}

bool liftMaxOpenFileLimit()
{
    struct rlimit fdLimit = {};
    if (getrlimit(RLIMIT_NOFILE, &fdLimit) < 0) {
        return false;
    }

    fdLimit.rlim_cur = fdLimit.rlim_max;
    if (setrlimit(RLIMIT_NOFILE, &fdLimit) < 0) {
        return false;
    }

    return true;
}
} // End of namespace
