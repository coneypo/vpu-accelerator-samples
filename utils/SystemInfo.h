
#pragma once

#include <iostream>

namespace HddlUnite {
namespace SystemInfo {
uint64_t totalRamInKB();
uint64_t freedRamInKB();
uint64_t usedRamInKB();
float loadAverage();
}
}
