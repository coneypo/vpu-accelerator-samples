//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//

#include <cstring>

#include "utils/StringUtils.h"

namespace StringUtils {
std::string alignLeft(const char* text, size_t width)
{
    std::string result;

    if (!text) {
        return result;
    }

    size_t len = strlen(text);

    if (width <= len) {
        return std::string(text);
    }

    result.append(text);
    result.append(width - len, ' ');

    return result;
}

std::string alignRight(const char* text, size_t width)
{
    std::string result;

    if (!text) {
        return result;
    }

    size_t len = strlen(text);

    if (width <= len) {
        return std::string(text);
    }

    result.append(width - len, ' ');
    result.append(text);

    return result;
}

std::string alignLeft(const std::string& text, size_t width)
{
    return alignLeft(text.c_str(), width);
}

std::string alignRight(const std::string& text, size_t width)
{
    return alignRight(text.c_str(), width);
}

std::string shortenText(std::string str, size_t width)
{
    std::string result;

    size_t nSize = str.size();

    if (nSize <= width) {
        return str;
    }

    if (width <= 7) {
        return str.substr(0, width);
    }

    auto subLen = (width - 3) / 2;

    result.append(str.substr(0, subLen));
    result.append("...");
    result.append(str.substr(nSize - subLen, subLen));

    return result;
}
}
