// Wall-clock benchmark of the serial and CPU-parallel batch solvers.
// Only the solve step is timed: batch generation, file I/O, rendering and
// compilation are excluded.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "batch.hpp"
#include "sysinfo.hpp"

namespace wb {

struct BenchRow {
    int workers = 1;
    std::vector<double> runsSeconds;   // measured repetitions (warm-up excluded)
    double medianSeconds = 0;
    double speedup = 1;                // T1 / Tp
    double efficiency = 1;             // speedup / p
    double maxDiffVsSerial = 0;        // max |T_serial - T_parallel| [degC]
};

struct BenchReport {
    SystemInfo system;
    BatchConfig config;
    int warmups = 1;
    int repetitions = 5;
    std::vector<BenchRow> rows;        // rows[0] is the serial (1 worker) baseline
    bool agreementOk = true;           // all diffs <= 1e-8 degC
    bool speedupAssessable = true;     // false on a single-logical-CPU machine
    bool cancelled = false;
    std::string note;
};

// progress(label, fraction in [0,1]) is optional.
BenchReport runBenchmark(const BatchConfig& cfg, int maxWorkers, int warmups, int repetitions,
                         BatchControl* ctl = nullptr,
                         const std::function<void(const std::string&, double)>& progress = {});

std::string benchCsv(const BenchReport& r);
std::string benchText(const BenchReport& r);

}  // namespace wb
