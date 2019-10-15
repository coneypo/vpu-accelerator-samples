/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>

namespace FileUtils {
bool exist(const char* path)
{
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

bool exist(const std::string& path)
{
    if (path.empty()) {
        return false;
    }
    return exist(path.c_str());
}

bool changeFileMode(const char* file, int mode)
{
    static_assert(sizeof(mode) >= sizeof(mode_t), "int is small than mode_t");
    if (!file) {
        errno = EINVAL;
        return false;
    }

    if (!exist(file)) {
        errno = EINVAL;
        return false;
    }

    if (mode >= 0) {
        if (chmod(file, (mode_t)mode) < 0) {
            return false;
        }
    }

    return true;
}

std::string readFile(const std::string& filePath)
{
    std::ifstream fs(filePath);
    if (!fs.is_open()) {
        std::cerr << "Error: open file " << filePath << " failed" << std::endl;
        return {};
    }

    fs.seekg(0, std::ios::end);
    size_t length = fs.tellg();
    if (!length) {
        std::cerr << "Error: file " << filePath << " is empty" << std::endl;
        return {};
    }

    std::string buffer(length, ' ');
    fs.seekg(0);
    fs.read(&buffer[0], length);

    return buffer;
}
}