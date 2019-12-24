//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include "utils/ShareMemory.h"
#include "utils/HLog.h"

#include <windows.h>

namespace HddlUnite {
ShareMemory::~ShareMemory()
{
    reset();
}

void* ShareMemory::create(const std::string& name, size_t size)
{
    m_mapHandle = ::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD)size, name.c_str());

    if (!m_mapHandle) {
        HError("Error: create share memory fail\n");
        return nullptr;
    }

    m_ptr = ::MapViewOfFile(m_mapHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);

    if (!m_ptr) {
        HError("Error: map share memory fail\n");
        BOOL stat = ::CloseHandle(m_mapHandle);

        if (!stat) {
            HError("Error: close share memory handle fail\n");
        }
        return nullptr;
    }
    m_name = name;
    m_size = size;
    return m_ptr;
}

void* ShareMemory::open(const std::string& name, size_t size)
{
    m_mapHandle = ::OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, name);

    if (!m_mapHandle) {
        HError("Error: open share memory fail\n");
        return nullptr;
    }

    m_ptr = ::MapViewOfFile(m_mapHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);

    if (!m_ptr) {
        HError("Error: map share memory fail\n");
        BOOL stat = ::CloseHandle(m_mapHandle);

        if (!stat) {
            HError("Error: close share memory handle fail\n");
        }
        return nullptr;
    }
    m_name = name;
    m_size = size;
    return m_ptr;
}

void ShareMemory::reset()
{
    if (!m_ptr) {
        return;
    }
    BOOL stat;
    stat = ::UnmapViewOfFile(m_ptr);

    if (!stat) {
        HError("Error: unmap fail\n");
    }

    stat = ::CloseHandle(m_mapHandle);

    if (!stat) {
        HError("Error: close share memory handle fail\n");
    }

    m_ptr = nullptr;
    m_mapHandle = nullptr;
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
