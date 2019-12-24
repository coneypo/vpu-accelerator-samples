//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_EXCEPTION_H
#define HDDLUNITE_EXCEPTION_H

#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

#include "utils/HLog.h"

namespace HddlUnite {
namespace Runtime {
class Exception : public std::exception {
public:
    Exception(std::string errorDesc, std::string filename, uint32_t line, std::string func)
        : m_errorDesc(std::move(errorDesc))
        , m_filename(std::move(filename))
        , m_function(std::move(func))
        , m_lineNum(line)
    {
    }

    const char* what() const noexcept override
    {
        return m_errorDesc.c_str();
    }

private:
    std::string m_errorDesc;
    std::string m_filename;
    std::string m_function;
    uint64_t m_lineNum { 0 };
};
}
}

#define THROW_BREAK_EXCEPTION(error) \
    throw HddlUnite::Runtime::Exception(error, __FILE__, __LINE__, __func__)

#define THROW_FATAL_EXCEPTION(...) \
    HFatal(__VA_ARGS__);           \
    throw std::runtime_error("HddlUnite Exception")

#endif //HDDLUNITE_EXCEPTION_H
