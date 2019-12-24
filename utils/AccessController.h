//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_ACCESSCONTROLLER_H
#define HDDLUNITE_ACCESSCONTROLLER_H

#include <string>

#include "utils/Singleton.h"

namespace HddlUnite {
class AccessController : public Singleton<AccessController> {
public:
    bool updateAccessAttribute(int fd);
    bool updateAccessAttribute(const char* file);
    bool updateAccessAttribute(std::string& file);

    std::tuple<std::string, std::string, int> getAccessAttribute();

    void setAccessAttribute(std::string user = "", std::string group = "users", int mode = 0660);

private:
    friend class Singleton<AccessController>;

    AccessController();
    ~AccessController() override = default;

private:
    std::string m_user;
    std::string m_group;
    uint64_t m_mode;
};
}

#endif //HDDLUNITE_ACCESSCONTROLLER_H
