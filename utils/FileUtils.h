//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_FILEUTILS_H
#define HDDLUNITE_FILEUTILS_H

#include <boost/filesystem.hpp>
#include <string>
#include <vector>

namespace FileUtils {
bool exist(const char* path);
bool exist(const std::string& path);

std::string getHome();
std::string getInstallDir();
std::string getEnv(const std::string& name);
std::string getAbsolutePath(const std::string& relativePath);

bool isFile(const std::string& filePath);
bool isDirectory(const std::string& filePath);
bool isAbsolutePath(const std::string& filePath);
bool createDirectories(const std::string& directory);

size_t getFileSize(const std::string& filePath);
std::string getFileName(const std::string& filePath);
std::string getFileNameStem(const std::string& filePath);

bool getPermissions(const std::string& filePath, uint64_t& perms);
bool setPermissions(const std::string& filePath, uint64_t perms);

std::string readFile(const char* filePath);
std::string readFile(const std::string& filePath);

bool writeFile(const std::string& destPath, const std::string& data);
bool writeFile(const std::string& destPath, const char* data, size_t dataSize);

bool removeFile(const std::string& filePath);
uint64_t removeAllFiles(const std::string& filePath);

std::string getTempDirectoryPath();
std::string getDirectoryOfExecuteFile();
std::vector<std::string> getFiles(const std::string& cate_dir);

bool liftMaxOpenFileLimit();
bool changeFileDesMode(int fd, int mode);
bool changeFileDesOwner(int fd, const char* user, const char* group);
bool changeFileOwner(const char* file, const char* user, const char* group);

bool updateAccessAttribute(int fd, std::string& group, std::string& user, int mode);
bool updateAccessAttribute(const std::string& file, std::string& group, std::string& user, uint64_t mode);

template <typename T1, typename T2>
boost::filesystem::path _joinPath(T1 father, T2 son)
{
    boost::filesystem::path f(father);
    boost::filesystem::path s(son);
    return f / s;
}

template <typename T, typename... Ts>
boost::filesystem::path _joinPath(T p1, Ts... p2)
{
    return boost::filesystem::path(p1) / _joinPath(p2...);
}

template <typename T, typename... Ts>
std::string joinPath(T p1, Ts... p2)
{
    auto r = _joinPath(p1, p2...);
    return r.string();
}
}

#endif //HDDLUNITE_FILEUTILS_H
