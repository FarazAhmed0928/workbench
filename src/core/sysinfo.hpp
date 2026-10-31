#pragma once
#include <string>

namespace wb {

struct SystemInfo {
    std::string cpuModel = "unknown";
    int physicalCores = 0;   // 0 = could not be determined
    int logicalCores = 0;
    double ramGiB = 0;
    std::string os;
    std::string compiler;
    std::string flags;       // optimisation flags baked in at build time
};

SystemInfo querySystemInfo();

}  // namespace wb
