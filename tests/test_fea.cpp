#include <cmath>
#include <cstdio>
#include <stdexcept>

#include "fea1d.hpp"

static int g_fail = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
            ++g_fail;                                                        \
        }                                                                    \
    } while (0)
#define CHECK_NEAR(a, b, tol) CHECK(std::fabs((a) - (b)) <= (tol))

int main() {
    using namespace wb;
    RodProblem p;

    // Boundary conditions are imposed exactly.
    for (int N : {1, 2, 10, 80}) {
        auto s = solveRod(p, N);
        CHECK_NEAR(s.T.front(), 100.0, 1e-12);
        CHECK_NEAR(s.T.back(), 20.0, 1e-12);
    }

    // Quadratic exact solution: linear FE is nodally exact in 1D.
    for (int N : {10, 20, 40, 80}) {
        auto e = computeErrors(p, solveRod(p, N));
        CHECK(e.maxNodalError < 1e-9);
        // Midpoint error = (q/(2k)) h^2 / 4 analytically.
        const double h = p.length / N;
        CHECK_NEAR(e.maxMidpointError, (p.q / (2 * p.k)) * h * h / 4.0, 1e-9);
    }

    // Convergence order 2 at midpoints.
    auto rows = convergenceStudy(p, {10, 20, 40, 80});
    CHECK(rows.size() == 4);
    for (size_t i = 1; i < rows.size(); ++i) CHECK_NEAR(rows[i].observedOrder, 2.0, 1e-6);

    // Zero generation: linear profile.
    RodProblem lin = p;
    lin.q = 0;
    auto s = solveRod(lin, 5);
    for (size_t i = 0; i < s.x.size(); ++i) CHECK_NEAR(s.T[i], 100.0 - 80.0 * s.x[i], 1e-10);

    // Negative tests.
    auto throws = [&](RodProblem q, int n) {
        try {
            solveRod(q, n);
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    RodProblem bad = p;
    bad.k = -1;
    CHECK(throws(bad, 10));
    bad = p;
    bad.k = 0;
    CHECK(throws(bad, 10));
    bad = p;
    bad.q = NAN;
    CHECK(throws(bad, 10));
    bad = p;
    bad.length = INFINITY;
    CHECK(throws(bad, 10));
    CHECK(throws(p, 0));
    CHECK(throws(p, -3));
    CHECK(!validate(bad, 10).empty());

    // CSV export has header + N+1 rows.
    auto csv = solutionCsv(p, solveRod(p, 10));
    int lines = 0;
    for (char c : csv) lines += (c == '\n');
    CHECK(lines == 12);

    if (g_fail == 0) std::printf("test_fea: all checks passed\n");
    return g_fail == 0 ? 0 : 1;
}
