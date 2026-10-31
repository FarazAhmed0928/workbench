#include <cmath>
#include <cstdio>

#include "batch.hpp"
#include "benchmark.hpp"

static int g_fail = 0;
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++g_fail;                                                   \
        }                                                               \
    } while (0)

int main() {
    using namespace wb;
    BatchConfig c;
    c.cases = 200;
    c.elements = 100;

    // Reproducible generator, positive parameters, different seed differs.
    auto a = generateBatch(c), b = generateBatch(c);
    CHECK(a.size() == 200);
    bool same = true, positive = true;
    for (size_t i = 0; i < a.size(); ++i) {
        same = same && a[i].k == b[i].k && a[i].q == b[i].q;
        positive = positive && a[i].k > 0 && a[i].q > 0;
    }
    CHECK(same && positive);
    BatchConfig c2 = c;
    c2.seed = 999;
    CHECK(generateBatch(c2)[0].k != a[0].k);

    // Serial == parallel (bitwise here) and input order preserved.
    auto s = solveSerial(a, c.elements);
    for (size_t workers : {1u, 2u, 3u, 4u, 8u}) {
        ThreadPool pool(workers);
        auto p = solveParallel(a, c.elements, pool);
        CHECK(p.completed && p.solved == 200);
        CHECK(maxAbsDifference(s, p) <= 1e-8);
    }
    // Spot check order: case 7 boundary values must match its own parameters.
    CHECK(std::fabs(s.temperatures[7].front() - a[7].tLeft) < 1e-12);
    CHECK(std::fabs(s.temperatures[7].back() - a[7].tRight) < 1e-12);

    // Cancellation leaves a clean, flagged result.
    BatchControl ctl;
    ctl.cancel = true;
    auto cx = solveSerial(a, c.elements, &ctl);
    CHECK(!cx.completed && cx.solved == 0);
    ThreadPool pool(2);
    auto cp = solveParallel(a, c.elements, pool, &ctl);
    CHECK(!cp.completed);

    // Config validation.
    BatchConfig bad;
    bad.cases = 0;
    CHECK(!validate(bad).empty());
    bad = c;
    bad.elements = 1;
    CHECK(!validate(bad).empty());

    // Benchmark harness produces consistent rows.
    BatchConfig small;
    small.cases = 100;
    small.elements = 200;
    auto rep = runBenchmark(small, 4, 1, 5);
    CHECK(rep.agreementOk);
    CHECK(!rep.rows.empty() && rep.rows[0].workers == 1);
    CHECK(rep.rows[0].runsSeconds.size() == 5);
    CHECK(std::fabs(rep.rows[0].speedup - 1.0) < 1e-12);
    for (auto& r : rep.rows) CHECK(r.workers <= std::max(4, 2));

    if (g_fail == 0) std::printf("test_batch: all checks passed\n");
    return g_fail == 0 ? 0 : 1;
}
