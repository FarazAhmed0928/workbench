// Batch of independent thermal solves: seeded generator, serial and parallel runners.
#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "fea1d.hpp"
#include "thread_pool.hpp"

namespace wb {

struct BatchConfig {
    std::uint64_t seed = 12345;
    int cases = 2000;
    int elements = 2000;      // mesh size per case
};

std::string validate(const BatchConfig& c);  // empty string if OK

// Deterministic across compilers/platforms: uses raw mt19937_64 output and a
// hand-written uniform mapping (std::uniform_real_distribution is NOT portable).
std::vector<RodProblem> generateBatch(const BatchConfig& c);

struct BatchResult {
    std::vector<std::vector<double>> temperatures;  // one vector per case, in INPUT order
    bool completed = true;                          // false if cancelled
    int solved = 0;
};

struct BatchControl {
    std::atomic<bool> cancel{false};
    std::atomic<int> done{0};   // progress counter for the UI
};

BatchResult solveSerial(const std::vector<RodProblem>& cases, int elements, BatchControl* ctl = nullptr);
// Uses an existing pool so pool construction is not part of compute timing.
BatchResult solveParallel(const std::vector<RodProblem>& cases, int elements, ThreadPool& pool,
                          BatchControl* ctl = nullptr);

double maxAbsDifference(const BatchResult& a, const BatchResult& b);

}  // namespace wb
