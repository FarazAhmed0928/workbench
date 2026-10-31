#include "fea1d.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace wb {

std::string validate(const RodProblem& p, int elements) {
    auto bad = [](double v) { return !std::isfinite(v); };
    if (elements < 1) return "Number of elements must be at least 1.";
    if (elements > 1000000) return "Number of elements must be at most 1,000,000.";
    if (bad(p.length) || p.length <= 0) return "Rod length L must be a positive finite number (m).";
    if (bad(p.area) || p.area <= 0) return "Cross-section area A must be a positive finite number (m^2).";
    if (bad(p.k) || p.k <= 0) return "Conductivity k must be a positive finite number (W/(m K)).";
    if (bad(p.q)) return "Heat generation q must be a finite number (W/m^3).";
    if (bad(p.tLeft) || bad(p.tRight)) return "Boundary temperatures must be finite numbers (degC).";
    return "";
}

RodSolution solveRod(const RodProblem& p, int N) {
    const std::string err = validate(p, N);
    if (!err.empty()) throw std::invalid_argument(err);

    const int n = N + 1;
    const double h = p.length / N;
    const double kAh = p.k * p.area / h;     // element stiffness factor
    const double fe = p.q * p.area * h / 2;  // consistent load per node of an element

    // Assemble global K (tridiagonal: sub, diag, super) and load vector f.
    std::vector<double> sub(n, 0.0), diag(n, 0.0), sup(n, 0.0), f(n, 0.0);
    for (int e = 0; e < N; ++e) {
        // ke = kAh * [[ 1, -1], [-1, 1]],  fe = [fe, fe]
        diag[e] += kAh;
        diag[e + 1] += kAh;
        sup[e] += -kAh;
        sub[e + 1] += -kAh;
        f[e] += fe;
        f[e + 1] += fe;
    }

    // Impose T(0)=tLeft and T(L)=tRight: move known values to the RHS of the
    // neighbouring rows, then replace the constrained rows by identity rows.
    f[1] -= sub[1] * p.tLeft;
    f[n - 2] -= sup[n - 2] * p.tRight;  // for N==1 handled below
    if (N == 1) {
        // single element: both nodes constrained, nothing to solve.
        RodSolution s;
        s.elements = 1;
        s.x = {0.0, p.length};
        s.T = {p.tLeft, p.tRight};
        return s;
    }
    sub[1] = 0.0;
    sup[n - 2] = 0.0;
    diag[0] = diag[n - 1] = 1.0;
    sup[0] = sub[n - 1] = 0.0;
    f[0] = p.tLeft;
    f[n - 1] = p.tRight;

    // Thomas algorithm.
    std::vector<double> c(n), d(n);
    c[0] = sup[0] / diag[0];
    d[0] = f[0] / diag[0];
    for (int i = 1; i < n; ++i) {
        const double m = diag[i] - sub[i] * c[i - 1];
        c[i] = sup[i] / m;
        d[i] = (f[i] - sub[i] * d[i - 1]) / m;
    }
    RodSolution s;
    s.elements = N;
    s.x.resize(n);
    s.T.resize(n);
    s.T[n - 1] = d[n - 1];
    for (int i = n - 2; i >= 0; --i) s.T[i] = d[i] - c[i] * s.T[i + 1];
    for (int i = 0; i < n; ++i) s.x[i] = h * i;
    s.x[n - 1] = p.length;
    return s;
}

double exactTemperature(const RodProblem& p, double x) {
    return p.tLeft + (p.tRight - p.tLeft) * x / p.length + (p.q / (2.0 * p.k)) * x * (p.length - x);
}

double interpolate(const RodSolution& s, double x) {
    const int N = s.elements;
    const double L = s.x.back();
    if (x <= 0) return s.T.front();
    if (x >= L) return s.T.back();
    int e = static_cast<int>(x / (L / N));
    e = std::min(e, N - 1);
    const double xi = (x - s.x[e]) / (s.x[e + 1] - s.x[e]);
    return (1.0 - xi) * s.T[e] + xi * s.T[e + 1];
}

ErrorReport computeErrors(const RodProblem& p, const RodSolution& s) {
    ErrorReport r;
    r.elements = s.elements;
    r.h = p.length / s.elements;
    for (size_t i = 0; i < s.x.size(); ++i)
        r.maxNodalError = std::max(r.maxNodalError, std::fabs(s.T[i] - exactTemperature(p, s.x[i])));
    for (int e = 0; e < s.elements; ++e) {
        const double xm = 0.5 * (s.x[e] + s.x[e + 1]);
        r.maxMidpointError = std::max(r.maxMidpointError, std::fabs(interpolate(s, xm) - exactTemperature(p, xm)));
    }
    return r;
}

std::vector<ConvergenceRow> convergenceStudy(const RodProblem& p, const std::vector<int>& meshes) {
    std::vector<ConvergenceRow> rows;
    for (int N : meshes) {
        ConvergenceRow row;
        row.err = computeErrors(p, solveRod(p, N));
        if (!rows.empty() && row.err.maxMidpointError > 0 && rows.back().err.maxMidpointError > 0)
            row.observedOrder = std::log(rows.back().err.maxMidpointError / row.err.maxMidpointError) /
                                std::log(rows.back().err.h / row.err.h);
        rows.push_back(row);
    }
    return rows;
}

std::string solutionCsv(const RodProblem& p, const RodSolution& s) {
    std::ostringstream o;
    o << std::setprecision(12);
    o << "x_m,T_fe_C,T_exact_C,abs_error_C\n";
    for (size_t i = 0; i < s.x.size(); ++i) {
        const double te = exactTemperature(p, s.x[i]);
        o << s.x[i] << ',' << s.T[i] << ',' << te << ',' << std::fabs(s.T[i] - te) << '\n';
    }
    return o.str();
}

std::string convergenceCsv(const std::vector<ConvergenceRow>& rows) {
    std::ostringstream o;
    o << std::setprecision(12);
    o << "elements,h_m,max_nodal_error_C,max_midpoint_error_C,observed_order\n";
    for (const auto& r : rows)
        o << r.err.elements << ',' << r.err.h << ',' << r.err.maxNodalError << ',' << r.err.maxMidpointError << ','
          << r.observedOrder << '\n';
    return o.str();
}

}  // namespace wb
