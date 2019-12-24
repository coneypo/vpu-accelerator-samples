//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include "AccessController.h"
#include "utils/FileUtils.h"

namespace HddlUnite {
AccessController::AccessController()
    : m_user()
    , m_group("users")
    , m_mode(0660)
{
}

bool AccessController::updateAccessAttribute(int fd)
{
    return FileUtils::updateAccessAttribute(fd, m_group, m_user, m_mode);
}

bool AccessController::updateAccessAttribute(std::string& file)
{
    return FileUtils::updateAccessAttribute(file, m_group, m_user, m_mode);
}

bool AccessController::updateAccessAttribute(const char* file)
{
    return FileUtils::updateAccessAttribute(file, m_group, m_user, m_mode);
}

std::tuple<std::string, std::string, int> AccessController::getAccessAttribute()
{
    return std::make_tuple(m_group, m_user, m_mode);
}

void AccessController::setAccessAttribute(std::string user, std::string group, int mode)
{
    m_user = std::move(user);
    m_group = std::move(group);
    m_mode = mode;
}
}
