//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef __HDDL_SECURITY_HELPER__
#define __HDDL_SECURITY_HELPER__

#include <memory>
#include <string>

namespace HddlUnite {
namespace SecurityUtils {
class Hasher {
public:
    Hasher();
    ~Hasher();

    void processBytes(const std::string& buffer);
    void processBytes(void const* buffer, size_t byteCount);
    std::string getHash();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

std::string getHash(const std::string& buffer);
std::string getHash(const void* buffer, size_t byteCount);
}
}

#endif
