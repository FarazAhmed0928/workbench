#include "batch.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace wb {

std::string validate(const BatchConfig& c) {
    if (c.cases < 1 || c.cases > 1000000) return "Case count must be between 1 and 1,000,000.";
    if (c.elements < 2 || c.elements > 100000) return "Mesh size must be between 2 and 100,000 elements.";
    return "";
}

namespace {
double unit(std::mt19937_64& g) { return static_cast<double>(g() >> 11) * (1.0 / 9007199254740992.0); }  // [0,1)
}

std::vector<RodProblem> generateBatch(const BatchConfig& c) {
    std::mt19937_64 g(c.seed);
    std::vector<RodProblem> v(static_cast<std::size_t>(c.cases));
    for (auto& p : v) {
        p.k = 1.0 + 199.0 * unit(g);        // 1..200 W/(m K), always positive
        p.q = 1000.0 + 99000.0 * unit(g);   // 1e3..1e5 W/m^3, always positive
        p.tLeft = 50.0 + 100.0 * unit(g);
        p.tRight = 0.0 + 50.0 * unit(g);
    }
    return v;
}

BatchResult solveSerial(const std::vector<RodProblem>& cases, int elements, BatchControl* ctl) {
    BatchResult r;
    r.temperatures.resize(cases.size());
    for (std::size_t i = 0; i < cases.size(); ++i) {
        if (ctl && ctl->cancel.load()) { r.completed = false; break; }
        r.temperatures[i] = solveRod(cases[i], elements).T;
        ++r.solved;
        if (ctl) ctl->done.store(r.solved);
    }
    return r;
}

BatchResult solveParallel(const std::vector<RodProblem>& cases, int elements, ThreadPool& pool, BatchControl* ctl) {
    BatchResult r;
    const std::size_t n = cases.size();
    r.temperatures.resize(n);
    std::vector<char> ok(n, 0);
    // Chunks keep queue overhead low; each case still writes only to its own slot.
    const std::size_t chunk = std::max<std::size_t>(1, n / (pool.size() * 8));
    for (std::size_t start = 0; start < n; start += chunk) {
        const std::size_t end = std::min(n, start + chunk);
        pool.submit([&, start, end] {
            for (std::size_t i = start; i < end; ++i) {
                if (ctl && ctl->cancel.load()) return;
                r.temperatures[i] = solveRod(cases[i], elements).T;
                ok[i] = 1;
                if (ctl) ctl->done.fetch_add(1);
            }
        });
    }
    pool.wait();
    for (char c : ok) r.solved += c;
    r.completed = (r.solved == static_cast<int>(n));
    return r;
}

double maxAbsDifference(const BatchResult& a, const BatchResult& b) {
    if (a.temperatures.size() != b.temperatures.size()) return INFINITY;
    double m = 0;
    for (std::size_t i = 0; i < a.temperatures.size(); ++i) {
        if (a.temperatures[i].size() != b.temperatures[i].size()) return INFINITY;
        for (std::size_t j = 0; j < a.temperatures[i].size(); ++j)
            m = std::max(m, std::fabs(a.temperatures[i][j] - b.temperatures[i][j]));
    }
    return m;
}

}  // namespace wb
