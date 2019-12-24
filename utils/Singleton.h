//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_SINGLETON_H
#define HDDLUNITE_SINGLETON_H

#include "NonCopyable.h"

template <typename Type>
class Singleton : public NonCopyable {
public:
    static Type* instance()
    {
        static Type obj;
        return &obj;
    }

protected:
    Singleton() = default;
    virtual ~Singleton() = default;
};

#endif //HDDLUNITE_SINGLETON_H
