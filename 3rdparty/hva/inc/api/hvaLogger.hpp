#ifndef HVA_LOGGER_HPP
#define HVA_LOGGER_HPP

#define ENABLE_LOG

#include <iostream>
#include <mutex>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <inc/util/hvaUtil.hpp>
#include <unistd.h>
#include <sys/syscall.h>

namespace hva{

using ms = std::chrono::milliseconds;

class hvaLogger_t{
public:
    enum LogLevel : unsigned {
        ERROR = 0x01,
        WARNING = 0x02,
        INFO = 0x04,
        DEBUG = 0x08,
        DISABLED = 0x00
    };

    hvaLogger_t(const hvaLogger_t&) = delete;
    hvaLogger_t& operator=(const hvaLogger_t&) = delete;

    ~hvaLogger_t();

    static hvaLogger_t& getInstance();

    template<typename... Args>
    void log(LogLevel level,const char* file, const char* func, long line, const char* msg, Args... args);

    void setLogLevel(unsigned logLevel);

    unsigned getLogLevel() const;

    void dumpToFile(const std::string& filename, bool dumpToFileOnly);

    void enableProfiling();

    void disableProfiling();

private:
    hvaLogger_t();

    unsigned m_logLevel;
    std::mutex m_mutex;
    std::ofstream m_oStream;
    bool m_enablePrintToConsole;
    bool m_enablePrintToLog;
    std::string m_filename;
    volatile bool m_enableProfiling;
};

template<typename... Args>
void hvaLogger_t::log(LogLevel level,const char* file, const char* func, long line, const char* msg, Args... args){
#ifdef ENABLE_LOG
    if(!(static_cast<unsigned>(level) & m_logLevel)){
        return;
    }
    std::stringstream stream;
    switch(level){
        case LogLevel::DEBUG:
            stream<< std::setw(9) << "[DEBUG]";
            break;
        case LogLevel::INFO:
            stream<< std::setw(9) << "[INFO]";
            break;
        case LogLevel::WARNING:
            stream<< std::setw(9) <<"[WARNING]";
            break;
        case LogLevel::ERROR:
            stream<< std::setw(9) <<"[ERROR]";
            break;
        default:
            stream<< std::setw(9) <<"[UNKOWN]";
            break;
    }
    if(m_enableProfiling){
        stream<<" ["<< syscall(SYS_gettid)<<"]";
        stream<<" ["<< (std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now().time_since_epoch())).count()<<"]";
    }
    stream<<": "<<file<<":"<<line<<" "<<func<<" "<<string_format(msg, args...);
        
    std::lock_guard<std::mutex> lg(m_mutex);

    if(m_enablePrintToConsole){
        std::cout<<stream.str()<<std::endl;
    }

    if(m_enablePrintToLog){
        m_oStream << stream.str()<<std::endl;
    }

#endif //#ifdef ENABLE_LOG
}

#define hvaLogger ::hva::hvaLogger_t::getInstance()

#define hva_setLogLevel(LEVEL) hvaLogger.setLogLevel(LEVEL)
#define hva_getLogLevel() hvaLogger.getLogLevel()

#ifdef ENABLE_LOG

#define LogPrint(logLevel, ...) \
    hvaLogger.log(logLevel,  __FILE__, __func__, __LINE__, __VA_ARGS__)

#define HVA_DEBUG(...) LogPrint(::hva::hvaLogger_t::LogLevel::DEBUG, __VA_ARGS__)
#define HVA_INFO(...) LogPrint(::hva::hvaLogger_t::LogLevel::INFO, __VA_ARGS__)
#define HVA_WARNING(...) LogPrint(::hva::hvaLogger_t::LogLevel::WARNING, __VA_ARGS__)
#define HVA_ERROR(...) LogPrint( ::hva::hvaLogger_t::LogLevel::ERROR, __VA_ARGS__)

#else //#ifdef ENABLE_LOG

#define LogPrint(logLevel, ...) 

#define HVA_DEBUG(...) 
#define HVA_INFO(...) 
#define HVA_WARNING(...) 
#define HVA_ERROR(...) 

#endif //#ifdef ENABLE_LOG

}

#endif //#ifndef HVA_LOGGER_HPP