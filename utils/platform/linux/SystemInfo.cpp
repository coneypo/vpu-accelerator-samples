
#include "utils/SystemInfo.h"
#include <sys/sysinfo.h>

namespace HddlUnite {

float SystemInfo::loadAverage()
{
    double load = 0.0;
    getloadavg(&load, sizeof(load) / sizeof(double));
    return static_cast<float>(load);
}

uint64_t SystemInfo::totalRamInKB()
{
    uint64_t size = 0;
    struct sysinfo info = {};
    sysinfo(&info);
    size = info.totalram * info.mem_unit;
    return size / 1024; // To kB
}

uint64_t SystemInfo::freedRamInKB()
{
    uint64_t size = 0;
    struct sysinfo info = {};
    sysinfo(&info);
    size = info.freeram * info.mem_unit;
    return size / 1024; // To kB
}

uint64_t SystemInfo::usedRamInKB()
{
    uint64_t size = 0;
    struct sysinfo info = {};
    sysinfo(&info);
    size = (info.totalram - info.freeram) * info.mem_unit;
    return size / 1024; // To kB
}

}
