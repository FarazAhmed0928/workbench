#include "backprop.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace wb {

BpForward forward(const BpParams& p, double x, double y) {
    BpForward f;
    f.z = p.w * x + p.b;
    f.h = std::tanh(f.z);
    f.yhat = p.v * f.h + p.c;
    const double d = f.yhat - y;
    f.loss = 0.5 * d * d;
    return f;
}

BpDetail backward(const BpParams& p, double x, double y) {
    BpDetail d;
    d.f = forward(p, x, y);
    d.e = d.f.yhat - y;             // dJ/dy_hat
    d.g.dv = d.e * d.f.h;           // dy_hat/dv = h
    d.g.dc = d.e;                   // dy_hat/dc = 1
    d.dJ_dh = d.e * p.v;            // dy_hat/dh = v
    d.dh_dz = 1.0 - d.f.h * d.f.h;  // d tanh(z)/dz = 1 - tanh^2(z)
    d.dJ_dz = d.dJ_dh * d.dh_dz;
    d.g.dw = d.dJ_dz * x;           // dz/dw = x
    d.g.db = d.dJ_dz;               // dz/db = 1
    return d;
}

BpGrad numericGradient(const BpParams& p, double x, double y, double eps) {
    auto J = [&](BpParams q) { return forward(q, x, y).loss; };
    BpGrad g;
    BpParams a = p, b = p;
    a.w += eps; b.w -= eps; g.dw = (J(a) - J(b)) / (2 * eps);
    a = b = p;
    a.b += eps; b.b -= eps; g.db = (J(a) - J(b)) / (2 * eps);
    a = b = p;
    a.v += eps; b.v -= eps; g.dv = (J(a) - J(b)) / (2 * eps);
    a = b = p;
    a.c += eps; b.c -= eps; g.dc = (J(a) - J(b)) / (2 * eps);
    return g;
}

double maxAbsGradError(const BpGrad& a, const BpGrad& b) {
    return std::max({std::fabs(a.dw - b.dw), std::fabs(a.db - b.db), std::fabs(a.dv - b.dv), std::fabs(a.dc - b.dc)});
}

BpParams gradientStep(const BpParams& p, const BpGrad& g, double lr) {
    return {p.w - lr * g.dw, p.b - lr * g.db, p.v - lr * g.dv, p.c - lr * g.dc};
}

BpTrace train(BpParams p, double x, double y, double lr, int steps) {
    BpTrace t;
    t.loss.push_back(forward(p, x, y).loss);
    for (int i = 0; i < steps; ++i) {
        p = gradientStep(p, backward(p, x, y).g, lr);
        t.loss.push_back(forward(p, x, y).loss);
    }
    t.finalParams = p;
    return t;
}

std::string backpropReport(const BpParams& p0, double x, double y, double lr) {
    const BpDetail d = backward(p0, x, y);
    const BpGrad n = numericGradient(p0, x, y);
    const BpParams p1 = gradientStep(p0, d.g, lr);
    std::ostringstream o;
    o << std::setprecision(10);
    o << "Inputs: x=" << x << " y=" << y << "  w=" << p0.w << " b=" << p0.b << " v=" << p0.v << " c=" << p0.c
      << "  lr=" << lr << "\n\nForward pass\n"
      << "  z      = w*x + b        = " << d.f.z << "\n"
      << "  h      = tanh(z)        = " << d.f.h << "\n"
      << "  y_hat  = v*h + c        = " << d.f.yhat << "\n"
      << "  J      = 0.5*(y_hat-y)^2 = " << d.f.loss << "\n\nChain rule\n"
      << "  dJ/dy_hat = y_hat - y   = " << d.e << "\n"
      << "  dJ/dv  = e*h            = " << d.g.dv << "\n"
      << "  dJ/dc  = e              = " << d.g.dc << "\n"
      << "  dJ/dh  = e*v            = " << d.dJ_dh << "\n"
      << "  dh/dz  = 1 - h^2        = " << d.dh_dz << "\n"
      << "  dJ/dz  = dJ/dh*dh/dz    = " << d.dJ_dz << "\n"
      << "  dJ/dw  = dJ/dz * x      = " << d.g.dw << "\n"
      << "  dJ/db  = dJ/dz          = " << d.g.db << "\n\nCentral differences (eps=1e-6)\n"
      << "  dw=" << n.dw << " db=" << n.db << " dv=" << n.dv << " dc=" << n.dc << "\n"
      << "  max abs error = " << std::scientific << maxAbsGradError(d.g, n) << std::fixed << "\n\n"
      << std::setprecision(10) << "Update (simultaneous, old gradients)\n"
      << "  w1=" << p1.w << " b1=" << p1.b << " v1=" << p1.v << " c1=" << p1.c << "\n"
      << "  J after update = " << forward(p1, x, y).loss << "\n";
    return o.str();
}

std::string lossCsv(const BpTrace& t) {
    std::ostringstream o;
    o << std::setprecision(12) << "step,loss\n";
    for (std::size_t i = 0; i < t.loss.size(); ++i) o << i << ',' << t.loss[i] << '\n';
    return o.str();
}

}  // namespace wb
