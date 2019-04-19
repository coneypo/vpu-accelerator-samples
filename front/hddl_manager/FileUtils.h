#ifndef _FILEUTILS_H
#define _FILEUTILS_H

#include <string>
namespace FileUtils {

bool exist(const char* path);

bool exist(const std::string& path);

bool changeFileMode(const char* file, int mode);

std::string readFile(const std::string& filePath);
}
#endif //_FILEUTILS_H
