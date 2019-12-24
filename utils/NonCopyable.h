//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_NONCOPYABLE_H
#define HDDLUNITE_NONCOPYABLE_H

class NonCopyable {
public:
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) = delete;

    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable& operator=(NonCopyable&&) = delete;

protected:
    NonCopyable() = default;
    virtual ~NonCopyable() = default;
};

#endif //HDDLUNITE_NONCOPYABLE_H
