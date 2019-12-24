//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_STRINGUTILS_H
#define HDDLUNITE_STRINGUTILS_H

#include <boost/format.hpp>
#include <sstream>
#include <string>

namespace StringUtils {
inline std::string formatString(boost::format& format)
{
    return format.str();
}

template <typename Type, typename... Args>
std::string formatString(boost::format& format, Type arg, Args... args)
{
    format % arg;

    return formatString(format, args...);
}

template <typename... Args>
std::string format(const char* fmt, Args... args)
{
    std::string result;

    try {
        boost::format format(fmt);
        result = formatString(format, args...);
    } catch (std::exception& exc) {
        return std::string(fmt) + "[BoostError: " + exc.what() + "]";
    }

    return result;
}

template <typename T>
void concat(std::stringstream& stream, T value)
{
    stream << value;
}

template <typename T, typename... Args>
void concat(std::stringstream& stream, T value, Args... args)
{
    stream << value;
    concat(stream, args...);
}

template <typename... Args>
std::string concat(Args... args)
{
    std::stringstream stream;
    concat(stream, args...);
    return stream.str();
}

// a safe replacement for strcpy, let compiler deduce destsz whenever possible
template <size_t destsz>
size_t copyStringSafe(char (&dest)[destsz], const std::string& str, size_t pos = 0)
{
    if (str.size() <= pos) {
        return 0;
    }
#ifdef WIN32
    strcpy_s(dest, destsz, str.c_str() + pos);
    return std::min<>(destsz - 1, str.length() - pos);
#else
    size_t cnt = str.copy(dest, destsz - 1, pos);
    cnt = std::min<size_t>(cnt, destsz - 1);
    dest[cnt] = '\0';
    return cnt;
#endif
}

std::string alignLeft(const char* text, size_t width);
std::string alignRight(const char* text, size_t width);
std::string alignLeft(const std::string& text, size_t width);
std::string alignRight(const std::string& text, size_t width);
std::string shortenText(std::string str, size_t width);
}

#endif //HDDLUNITE_STRINGUTILS_H
