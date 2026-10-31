#include <cmath>
#include <cstdio>

#include "backprop.hpp"

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
    BpParams p;  // w=0.5 b=0 v=1 c=0
    auto d = backward(p, 1.0, 2.0);

    // Independent hand values: z=0.5, h=tanh(0.5), yhat=h, e=h-2.
    const double h = std::tanh(0.5);
    CHECK(std::fabs(d.f.h - h) < 1e-15);
    CHECK(std::fabs(d.e - (h - 2.0)) < 1e-15);
    CHECK(std::fabs(d.g.dv - (h - 2.0) * h) < 1e-15);
    CHECK(std::fabs(d.g.dc - (h - 2.0)) < 1e-15);
    CHECK(std::fabs(d.g.dw - (h - 2.0) * (1 - h * h)) < 1e-15);
    CHECK(std::fabs(d.g.db - d.g.dw) < 1e-15);   // x = 1

    // Gradient check: central differences, eps 1e-6, tolerance 1e-5.
    CHECK(maxAbsGradError(d.g, numericGradient(p, 1.0, 2.0, 1e-6)) <= 1e-5);

    // Descent direction: a small step must reduce the loss.
    auto p1 = gradientStep(p, d.g, 0.1);
    CHECK(forward(p1, 1.0, 2.0).loss < forward(p, 1.0, 2.0).loss);

    // Update uses old gradients: recompute w1 explicitly.
    CHECK(std::fabs(p1.w - (0.5 - 0.1 * d.g.dw)) < 1e-15);
    CHECK(std::fabs(p1.v - (1.0 - 0.1 * d.g.dv)) < 1e-15);

    // 100 steps: monotone non-increasing loss and clearly smaller than start.
    auto t = train(p, 1.0, 2.0, 0.1, 100);
    CHECK(t.loss.size() == 101);
    bool mono = true;
    for (size_t i = 1; i < t.loss.size(); ++i) mono = mono && t.loss[i] <= t.loss[i - 1] + 1e-15;
    CHECK(mono);
    CHECK(t.loss.back() < 0.01 * t.loss.front());

    // Also check gradients at a different point (not just the start).
    BpParams q{0.3, -0.2, 0.7, 0.1};
    CHECK(maxAbsGradError(backward(q, -0.8, 1.5).g, numericGradient(q, -0.8, 1.5)) <= 1e-5);

    if (g_fail == 0) std::printf("test_backprop: all checks passed\n");
    return g_fail == 0 ? 0 : 1;
}
