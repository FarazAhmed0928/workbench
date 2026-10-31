// 1D steady-state heat conduction, two-node linear finite elements.
// Solves  -d/dx(k A dT/dx) = q A  on [0, L] with T(0)=T0, T(L)=TL.
// Units: m, m^2, W/(m K), W/m^3, degC.
#pragma once
#include <string>
#include <vector>

namespace wb {

struct RodProblem {
    double length = 1.0;       // L  [m]
    double area = 1e-4;        // A  [m^2]
    double k = 10.0;           // conductivity [W/(m K)]
    double q = 10000.0;        // volumetric heat generation [W/m^3]
    double tLeft = 100.0;      // T(0) [degC]
    double tRight = 20.0;      // T(L) [degC]
};

struct RodSolution {
    int elements = 0;
    std::vector<double> x;     // node coordinates, size N+1
    std::vector<double> T;     // nodal temperatures, size N+1
};

// Returns an empty string if valid, otherwise an actionable message.
std::string validate(const RodProblem& p, int elements);

// Assemble K and f, impose Dirichlet BCs by row/column elimination, solve.
// Throws std::invalid_argument if validate() fails.
RodSolution solveRod(const RodProblem& p, int elements);

double exactTemperature(const RodProblem& p, double x);

// FE interpolation (piecewise linear) of the solution at x.
double interpolate(const RodSolution& s, double x);

struct ErrorReport {
    int elements = 0;
    double h = 0.0;
    double maxNodalError = 0.0;     // max |T_h - T_exact| at nodes
    double maxMidpointError = 0.0;  // max |FE interpolation - T_exact| at element midpoints
};

ErrorReport computeErrors(const RodProblem& p, const RodSolution& s);

// Convergence study; also fills observed order between successive meshes.
struct ConvergenceRow {
    ErrorReport err;
    double observedOrder = 0.0;     // 0 for the first row
};
std::vector<ConvergenceRow> convergenceStudy(const RodProblem& p, const std::vector<int>& meshes);

// CSV helpers (raw data behind the plots).
std::string solutionCsv(const RodProblem& p, const RodSolution& s);
std::string convergenceCsv(const std::vector<ConvergenceRow>& rows);

}  // namespace wb
