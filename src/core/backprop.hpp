// Task F: one-sample regression network, hand-derived gradients (no autodiff).
//   z = w x + b,  h = tanh(z),  y_hat = v h + c,  J = 0.5 (y_hat - y)^2
#pragma once
#include <string>
#include <vector>

namespace wb {

struct BpParams {
    double w = 0.5, b = 0.0, v = 1.0, c = 0.0;
};

struct BpForward {
    double z = 0, h = 0, yhat = 0, loss = 0;
};

struct BpGrad {
    double dw = 0, db = 0, dv = 0, dc = 0;
};

struct BpDetail {          // every intermediate used in the chain rule
    BpForward f;
    double e = 0;          // dJ/dy_hat = y_hat - y
    double dJ_dh = 0;      // e * v
    double dh_dz = 0;      // 1 - h^2
    double dJ_dz = 0;      // dJ_dh * dh_dz
    BpGrad g;
};

BpForward forward(const BpParams& p, double x, double y);
BpDetail backward(const BpParams& p, double x, double y);

// Central finite differences, used only to check the analytic gradients.
BpGrad numericGradient(const BpParams& p, double x, double y, double eps = 1e-6);
double maxAbsGradError(const BpGrad& a, const BpGrad& b);

// Simultaneous update: all four gradients come from the SAME (old) parameters.
BpParams gradientStep(const BpParams& p, const BpGrad& g, double lr);

struct BpTrace {
    std::vector<double> loss;      // loss[0] = initial loss, loss[i] = after i updates
    BpParams finalParams;
};
BpTrace train(BpParams p, double x, double y, double lr, int steps);

std::string backpropReport(const BpParams& p0, double x, double y, double lr);
std::string lossCsv(const BpTrace& t);

}  // namespace wb
