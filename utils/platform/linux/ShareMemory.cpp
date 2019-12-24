//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utils/HLog.h"
#include "utils/ShareMemory.h"

namespace HddlUnite {
ShareMemory::~ShareMemory()
{
    reset();
}

void* ShareMemory::create(const std::string& name, size_t size)
{
    if (open(name, size, O_CREAT | O_RDWR)) {
        m_isOwner = true;
        m_name = name;
        return m_ptr;
    }

    return nullptr;
}

void* ShareMemory::open(const std::string& name, size_t size)
{
    if (open(name, size, O_RDWR)) {
        m_name = name;
        return m_ptr;
    }

    return nullptr;
}

bool ShareMemory::open(const std::string& name, size_t size, int oflag)
{
    if (name.empty()) {
        HError("Error: empty name for create share memory\n");
        return false;
    }

    int shmFd = shm_open(name.c_str(), oflag, 0666);

    if (shmFd < 0) {
        HError("Error: shm_open() failed: errno=%d (%s)\n", errno, strerror(errno));
        return false;
    }

    if (oflag & O_CREAT) {
        if (ftruncate(shmFd, size) < 0) {
            HError("Error: ftruncate() failed: errno=%d (%s)\n", errno, strerror(errno));
            close(shmFd);
            shm_unlink(name.c_str());
            return false;
        }
    }

    void* base = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);

    if (base == MAP_FAILED) {
        HError("Error: mmap() failed: errno=%d (%s)\n", errno, strerror(errno));
        close(shmFd);
        shm_unlink(name.c_str());
        return false;
    }

    if (close(shmFd) == -1) {
        HError("Error: close() failed: errno=%d (%s)\n", errno, strerror(errno));
        munmap(base, size);
        shm_unlink(name.c_str());
        return false;
    }

    m_ptr = base;
    m_name = name;
    m_size = size;

    return true;
}

void ShareMemory::reset()
{
    if (!m_ptr) {
        return;
    }

    if (munmap(m_ptr, m_size) < 0) {
        HError("Error: munmap() failed: error=%d (%s)\n", errno, strerror(errno));
    }

    if (m_isOwner) {
        if (shm_unlink(m_name.c_str()) == -1) {
            HError("Error: shm_unlink() failed: error=%d (%s)\n", errno, strerror(errno));
        }
    }

    m_isOwner = false;
    m_ptr = nullptr;
    m_name = "";
    m_size = 0;
}

void* ShareMemory::getData()
{
    return m_ptr;
}

size_t ShareMemory::getSize()
{
    return m_size;
}

std::string ShareMemory::getName() const
{
    return m_name;
}
}
