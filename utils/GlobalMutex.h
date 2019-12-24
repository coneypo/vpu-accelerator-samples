//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_GLOBALMUTEX_H
#define HDDLUNITE_GLOBALMUTEX_H

#include "utils/Mutex.h"

namespace HddlUnite {
class GlobalMutex {
public:
    explicit GlobalMutex(std::string name, bool closeOnExec = false);
    virtual ~GlobalMutex();

    bool lock();
    bool lock(long timeoutMilliseconds);
    bool trylock();
    void unlock();

private:
    Mutex m_mutex;
    std::string m_name;
    int m_fd; // for linux impl
    void* m_globalMutex; // for win32 impl
};
}

#endif //HDDLUNITE_GLOBALMUTEX_H
