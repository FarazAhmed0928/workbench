#include "sysinfo.hpp"

#include <cstdint>
#include <fstream>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#else
#include <unistd.h>
#endif

#ifndef WB_BUILD_FLAGS
#define WB_BUILD_FLAGS "unknown"
#endif

namespace wb {

SystemInfo querySystemInfo() {
    SystemInfo s;
    s.logicalCores = static_cast<int>(std::thread::hardware_concurrency());

#if defined(_MSC_VER)
    s.compiler = "MSVC " + std::to_string(_MSC_VER);
#elif defined(__clang__)
    s.compiler = std::string("Clang ") + __clang_version__;
#elif defined(__GNUC__)
    s.compiler = std::string("GCC ") + __VERSION__;
#endif
    s.flags = WB_BUILD_FLAGS;

#if defined(_WIN32)
    s.os = "Windows";
    MEMORYSTATUSEX m{};
    m.dwLength = sizeof(m);
    if (GlobalMemoryStatusEx(&m)) s.ramGiB = static_cast<double>(m.ullTotalPhys) / (1024.0 * 1024 * 1024);
    char buf[256];
    DWORD sz = sizeof(buf);
    if (RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString",
                     RRF_RT_REG_SZ, nullptr, buf, &sz) == ERROR_SUCCESS)
        s.cpuModel = buf;
    DWORD len = 0;
    GetLogicalProcessorInformation(nullptr, &len);
    if (len > 0) {
        std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> info(len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
        if (GetLogicalProcessorInformation(info.data(), &len))
            for (auto& i : info)
                if (i.Relationship == RelationProcessorCore) ++s.physicalCores;
    }
#elif defined(__APPLE__)
    s.os = "macOS";
    char buf[256];
    size_t sz = sizeof(buf);
    if (sysctlbyname("machdep.cpu.brand_string", buf, &sz, nullptr, 0) == 0) s.cpuModel = buf;
    std::uint64_t mem = 0;
    sz = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &sz, nullptr, 0) == 0) s.ramGiB = static_cast<double>(mem) / (1024.0 * 1024 * 1024);
    int phys = 0;
    sz = sizeof(phys);
    if (sysctlbyname("hw.physicalcpu", &phys, &sz, nullptr, 0) == 0) s.physicalCores = phys;
#else
    s.os = "Linux";
    const long pages = sysconf(_SC_PHYS_PAGES), psz = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && psz > 0) s.ramGiB = static_cast<double>(pages) * psz / (1024.0 * 1024 * 1024);
    std::ifstream f("/proc/cpuinfo");
    std::string line, phys = "0";
    std::set<std::string> cores;
    while (std::getline(f, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon), val = line.substr(colon + 2 <= line.size() ? colon + 2 : colon + 1);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        if (key == "model name" && s.cpuModel == "unknown") s.cpuModel = val;
        else if (key == "physical id") phys = val;
        else if (key == "core id") cores.insert(phys + ":" + val);
    }
    s.physicalCores = static_cast<int>(cores.size());
#endif
    return s;
}

}  // namespace wb
