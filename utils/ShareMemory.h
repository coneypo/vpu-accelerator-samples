//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_SHAREMEMORY_H
#define HDDLUNITE_SHAREMEMORY_H

#include <string>

namespace HddlUnite {
class ShareMemory {
public:
    ~ShareMemory();

    void* create(const std::string& name, size_t size);
    void* open(const std::string& name, size_t size);
    void reset();

    void* getData();
    size_t getSize();
    std::string getName() const;

protected:
    bool open(const std::string& name, size_t size, int oflag);

private:
    std::string m_name;
    size_t m_size { 0 };
    void* m_ptr { nullptr };
    void* m_mapHandle { nullptr }; // used in windows implementation
    bool m_isOwner { false }; // used in linux implementation
};
}

#endif //HDDLUNITE_SHAREMEMORY_H
