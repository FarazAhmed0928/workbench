#include "benchmark.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <memory>
#include <sstream>

namespace wb {
namespace {

using Clock = std::chrono::steady_clock;

double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

}  // namespace

BenchReport runBenchmark(const BatchConfig& cfg, int maxWorkers, int warmups, int repetitions, BatchControl* ctl,
                         const std::function<void(const std::string&, double)>& progress) {
    BenchReport rep;
    rep.system = querySystemInfo();
    rep.config = cfg;
    rep.warmups = warmups;
    rep.repetitions = repetitions;

    const int logical = std::max(1, rep.system.logicalCores);
    const int cap = std::max(1, std::min(maxWorkers, logical));
    std::vector<int> counts = {1};
    for (int p : {2, 4})
        if (p <= cap) counts.push_back(p);
    rep.speedupAssessable = logical > 1;
    if (!rep.speedupAssessable) {
        // Still exercise the parallel path with 2 threads, but speedup cannot be judged.
        counts.push_back(2);
        rep.note = "Only one logical CPU is available: the parallel path was run (2 threads) to prove correctness, "
                   "but speedup cannot be assessed on this machine.";
    }

    // The data set is generated once, outside every timed region.
    const auto cases = generateBatch(cfg);
    const int N = cfg.elements;
    const double totalSteps = static_cast<double>(counts.size()) * (warmups + repetitions);
    double stepsDone = 0;
    auto tick = [&](const std::string& label) {
        stepsDone += 1;
        if (progress) progress(label, stepsDone / totalSteps);
    };

    BatchResult serialRef = solveSerial(cases, N);  // reference for agreement checks

    for (int p : counts) {
        BenchRow row;
        row.workers = p;
        const std::string label = std::to_string(p) + (p == 1 ? " worker (serial)" : " workers");
        std::unique_ptr<ThreadPool> pool;
        if (p > 1) pool.reset(new ThreadPool(static_cast<std::size_t>(p)));  // created outside the timed region

        auto once = [&]() { return p == 1 ? solveSerial(cases, N) : solveParallel(cases, N, *pool); };

        for (int w = 0; w < warmups; ++w) {
            if (ctl && ctl->cancel.load()) { rep.cancelled = true; return rep; }
            once();
            tick(label + " warm-up");
        }
        BatchResult last;
        for (int i = 0; i < repetitions; ++i) {
            if (ctl && ctl->cancel.load()) { rep.cancelled = true; return rep; }
            const auto t0 = Clock::now();
            last = once();
            const auto t1 = Clock::now();
            row.runsSeconds.push_back(std::chrono::duration<double>(t1 - t0).count());
            tick(label + " run " + std::to_string(i + 1));
        }
        row.medianSeconds = median(row.runsSeconds);
        row.maxDiffVsSerial = maxAbsDifference(serialRef, last);
        if (row.maxDiffVsSerial > 1e-8) rep.agreementOk = false;
        rep.rows.push_back(row);
    }

    const double t1 = rep.rows.front().medianSeconds;
    for (auto& r : rep.rows) {
        r.speedup = r.medianSeconds > 0 ? t1 / r.medianSeconds : 0;
        r.efficiency = r.speedup / r.workers;
    }
    return rep;
}

std::string benchCsv(const BenchReport& r) {
    std::ostringstream o;
    o << std::setprecision(9);
    o << "workers,median_s,run1_s,run2_s,run3_s,run4_s,run5_s,speedup_T1_over_Tp,efficiency,max_abs_diff_vs_serial_C\n";
    for (const auto& row : r.rows) {
        o << row.workers << ',' << row.medianSeconds;
        for (std::size_t i = 0; i < 5; ++i) o << ',' << (i < row.runsSeconds.size() ? row.runsSeconds[i] : 0.0);
        o << ',' << row.speedup << ',' << row.efficiency << ',' << row.maxDiffVsSerial << '\n';
    }
    return o.str();
}

std::string benchText(const BenchReport& r) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(4);
    o << "CPU: " << r.system.cpuModel << "\n"
      << "Cores: " << r.system.physicalCores << " physical / " << r.system.logicalCores << " logical\n"
      << "RAM: " << std::setprecision(1) << r.system.ramGiB << std::setprecision(4) << " GiB\n"
      << "OS: " << r.system.os << "\nCompiler: " << r.system.compiler << "\nFlags: " << r.system.flags << "\n"
      << "Workload: " << r.config.cases << " cases x " << r.config.elements << " elements, seed " << r.config.seed << "\n"
      << "Protocol: " << r.warmups << " warm-up, " << r.repetitions << " measured runs, median reported\n\n"
      << "workers  median[s]  speedup  efficiency  max|diff|[C]\n";
    for (const auto& row : r.rows)
        o << std::setw(7) << row.workers << std::setw(11) << row.medianSeconds << std::setw(9) << row.speedup
          << std::setw(12) << row.efficiency << "  " << std::scientific << row.maxDiffVsSerial << std::fixed << "\n";
    o << "\nSerial/parallel agreement (<= 1e-8 C): " << (r.agreementOk ? "PASS" : "FAIL") << "\n";
    if (!r.note.empty()) o << "Note: " << r.note << "\n";
    return o.str();
}

}  // namespace wb
