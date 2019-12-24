//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include <cstring>
#include <fstream>
#include <iostream>

#include "utils/FileUtils.h"
#include "utils/HLog.h"

namespace FileUtils {
bool exist(const char* filePath)
{
    try {
        return boost::filesystem::exists(filePath);
    } catch (boost::filesystem::filesystem_error& e) {
        HError("Error: exist(%s) failed: %s", filePath, e.what());
    }

    return false;
}

bool exist(const std::string& path)
{
    if (path.empty()) {
        return false;
    }
    return exist(path.c_str());
}

std::string getEnv(const std::string& name)
{
    char* homePath = getenv(name.c_str());
    if (homePath) {
        return homePath;
    }
    return {};
}

std::string getInstallDir()
{
    auto installDir = getEnv("KMB_INSTALL_DIR");
    if (installDir.empty()) {
        std::cout << "KMB_INSTALL_DIR is not set, use its default value (/usr/local)" << std::endl;
        installDir = "/usr/local";
    }
    return installDir;
}

bool isFile(const std::string& filePath)
{
    try {
        return boost::filesystem::is_regular_file(filePath);
    } catch (boost::filesystem::filesystem_error& e) {
        HError("Error: isFile(%s) failed: %s", filePath, e.what());
    }

    return false;
}

bool isDirectory(const std::string& filePath)
{
    try {
        return boost::filesystem::is_directory(filePath);
    } catch (boost::filesystem::filesystem_error& e) {
        HError("Error: isDirectory(%s) failed: %s", filePath, e.what());
    }

    return false;
}

bool createDirectories(const std::string& directory)
{
    try {
        return boost::filesystem::create_directories(directory.c_str());
    } catch (boost::filesystem::filesystem_error& e) {
        HError("Error: createDirectories() failed: %s", e.what());
    }

    return false;
}

bool isAbsolutePath(const std::string& filePath)
{
    try {
        boost::filesystem::path path(filePath);
        return path.is_absolute();
    } catch (boost::filesystem::filesystem_error& e) {
        HError("Error: IsAbsolutePath(%s) failed: %s", filePath, e.what());
    }

    return false;
}

std::string getAbsolutePath(const std::string& relativePath)
{
    try {
        return boost::filesystem::absolute(relativePath).string();
    } catch (boost::filesystem::filesystem_error& e) {
        HError("Error: getAbsolutePath(%s) failed: %s", relativePath, e.what());
    }

    return {};
}

bool getPermissions(const std::string& filePath, uint64_t& perms)
{
    try {
        auto status = boost::filesystem::status(filePath);
        perms = status.permissions();
        return true;
    } catch (boost::filesystem::filesystem_error& e) {
        HError("Error: getPermissions(%s) failed: %s", filePath, e.what());
    }

    return false;
}

bool setPermissions(const std::string& filePath, const uint64_t perms)
{
    try {
        boost::filesystem::permissions(filePath, static_cast<boost::filesystem::perms>(perms));
        return true;
    } catch (boost::filesystem::filesystem_error& e) {
        HError("Error: setPermissions(%s) failed: %s", filePath, e.what());
    }

    return false;
}

std::string readFile(const char* filePath)
{
    std::ifstream in(filePath, std::ios::in | std::ios::binary);
    if (!in) {
        HError("open file \"%s\" failed", filePath);
        return {};
    }

    std::string content;

    in.seekg(0, std::ios::end);
    content.resize(in.tellg());
    in.seekg(0, std::ios::beg);

    try {
        in.read(&content[0], content.size());
    } catch (...) {
        HError("read file \"%s\" failed", filePath);
        return {};
    }

    return content;
}

std::string readFile(const std::string& filePath)
{
    return filePath.empty() ? std::string() : readFile(filePath.c_str());
}

bool writeFile(const std::string& destPath, const std::string& data)
{
    return data.empty() ? false : writeFile(destPath, data.data(), data.size());
}

bool writeFile(const std::string& destPath, const char* data, size_t dataSize)
{
    if (destPath.empty() || !data || !dataSize) {
        return false;
    }

    std::ofstream out(destPath, std::ios::out | std::ios::binary);
    if (!out) {
        HError("open file \"%s\" failed", destPath);
        return false;
    }

    try {
        out.write(data, dataSize);
    } catch (...) {
        HError("write file \"%s\" failed", destPath);
        return false;
    }

    return true;
}

bool removeFile(const std::string& filePath)
{
    try {
        boost::filesystem::path path(filePath);
        return boost::filesystem::remove(filePath);
    } catch (boost::filesystem::filesystem_error& e) {
        HError("Error: remove file (%s) failed: %s", filePath, e.what());
    }

    return false;
}

uint64_t removeAllFiles(const std::string& filePath)
{
    try {
        boost::filesystem::path path(filePath);
        return boost::filesystem::remove_all(filePath);
    } catch (boost::filesystem::filesystem_error& e) {
        HError("Error: remove file (%s) failed: %s", filePath, e.what());
    }

    return 0;
}

std::string getTempDirectoryPath()
{
    auto tempPath = boost::filesystem::temp_directory_path();

    return boost::filesystem::canonical(tempPath).string();
}

std::string getFileName(const std::string& filePath)
{
    return boost::filesystem::path(filePath).filename().string();
}

std::string getFileNameStem(const std::string& filePath)
{
    return boost::filesystem::path(filePath).stem().string();
}

size_t getFileSize(const std::string& filePath)
{
    std::ifstream in(filePath, std::ios::in | std::ios::binary);
    if (!in) {
        HError("open file \"%s\" failed", filePath);
        return 0;
    }

    in.seekg(0, std::ios::end);

    return in.tellg();
}

std::vector<std::string> getFiles(const std::string& path)
{
    if (!isDirectory(path)) {
        HError("Error: invalid directory '%s'", path);
        return {};
    }

    std::vector<std::string> files;

    try {
        boost::filesystem::directory_iterator iter(path), end;
        while (iter != end) {
            if (boost::filesystem::is_regular_file(iter->status())) {
                auto filepath = iter->path().string();
                files.push_back(filepath);
            }
            ++iter;
        }
    } catch (boost::filesystem::filesystem_error& e) {
        HError("Error: getFiles(%s) failed: %s", path, e.what());
        return {};
    }

    std::sort(files.begin(), files.end());

    return files;
}

bool updateAccessAttribute(int fd, std::string& group, std::string& user, int mode)
{
    const char* userPtr = user.empty() ? nullptr : user.c_str();
    const char* groupPtr = group.empty() ? nullptr : group.c_str();

    if (!changeFileDesOwner(fd, userPtr, groupPtr)) {
        HError("Error: Failed to set owner to fd: %d", fd);
        return false;
    }

    if (!changeFileDesMode(fd, mode)) {
        HError("Error: Failed to set mode to fd: %d", fd);
        return false;
    }

    HInfo("Set fd:%d owner: user-'%s', group-'%s', mode-'0%o'",
        fd, user.empty() ? "no_change" : user, group.empty() ? "no_change" : group, mode);

    return true;
}

bool updateAccessAttribute(const std::string& file, std::string& group, std::string& user, const uint64_t mode)
{
    const char* userPtr = user.empty() ? nullptr : user.c_str();
    const char* groupPtr = group.empty() ? nullptr : group.c_str();

    if (!changeFileOwner(file.c_str(), userPtr, groupPtr)) {
        HError("Error: Failed to set owner to file: %s", file);
        return false;
    }

    if (!setPermissions(file, static_cast<boost::filesystem::perms>(mode))) {
        HError("Error: Failed to set owner and mode to file: %s", file);
        return false;
    }

    HDebug("Set file:%s owner: user-'%s', group-'%s', mode-'0%o'",
        file, user.empty() ? "no_change" : user, group.empty() ? "no_change" : group, mode);

    return true;
}
} // End of namespace
