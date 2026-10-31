// Regenerates the deterministic evidence in results/ (FEA and backprop).
// Benchmark results are produced separately by `bench` on the target machine.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

#include "backprop.hpp"
#include "fea1d.hpp"
#include "svgplot.hpp"

static void write(const std::string& path, const std::string& text) {
    std::ofstream(path, std::ios::binary) << text;
    std::printf("wrote %s\n", path.c_str());
}

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : "results";
    using namespace wb;

    // ---- Task C ----
    RodProblem p;
    const std::vector<int> meshes = {10, 20, 40, 80};
    PlotSpec temp;
    temp.title = "Rod temperature: FE solutions vs analytical (L=1 m, k=10 W/(m K), q=10 kW/m^3)";
    temp.xLabel = "x [m]";
    temp.yLabel = "Temperature T [degC]";
    PlotSeries exact{"exact", {}, {}, false};
    for (int i = 0; i <= 200; ++i) {
        const double x = i / 200.0;
        exact.x.push_back(x);
        exact.y.push_back(exactTemperature(p, x));
    }
    temp.series.push_back(exact);
    for (int N : meshes) {
        auto s = solveRod(p, N);
        write(dir + "/fea_solution_N" + std::to_string(N) + ".csv", solutionCsv(p, s));
        temp.series.push_back({"FE N=" + std::to_string(N), s.x, s.T, N == 10});
    }
    write(dir + "/fea_temperature.svg", svgLinePlot(temp));

    // Pointwise error of the FE interpolation: the curves above overlap, this makes the difference visible.
    PlotSpec errp;
    errp.title = "FE interpolation error |T_FE(x) - T_exact(x)|";
    errp.xLabel = "x [m]";
    errp.yLabel = "Absolute error [degC]";
    for (int N : meshes) {
        auto s = solveRod(p, N);
        PlotSeries se{"N=" + std::to_string(N), {}, {}, false};
        for (int i = 0; i <= 800; ++i) {
            const double x = i / 800.0;
            se.x.push_back(x);
            se.y.push_back(std::fabs(interpolate(s, x) - exactTemperature(p, x)));
        }
        errp.series.push_back(se);
    }
    write(dir + "/fea_error.svg", svgLinePlot(errp));

    auto rows = convergenceStudy(p, meshes);
    write(dir + "/fea_convergence.csv", convergenceCsv(rows));
    PlotSpec conv;
    conv.title = "Mesh convergence: max midpoint error (FE interpolation)";
    conv.xLabel = "Element size h [m]";
    conv.yLabel = "Max |T_FE - T_exact| at midpoints [degC]";
    conv.logX = conv.logY = true;
    PlotSeries ms{"midpoint error (slope 2)", {}, {}, true};
    for (auto& r : rows) { ms.x.push_back(r.err.h); ms.y.push_back(r.err.maxMidpointError); }
    conv.series.push_back(ms);
    write(dir + "/fea_convergence.svg", svgLinePlot(conv));

    // ---- Task F ----
    BpParams p0;
    write(dir + "/backprop_report.txt", backpropReport(p0, 1.0, 2.0, 0.1));
    auto tr = train(p0, 1.0, 2.0, 0.1, 100);
    write(dir + "/backprop_loss.csv", lossCsv(tr));
    PlotSpec lp;
    lp.title = "Backpropagation: loss over 100 gradient-descent steps (lr = 0.1)";
    lp.xLabel = "Training step";
    lp.yLabel = "Loss J (dimensionless)";
    lp.logY = true;
    PlotSeries ls{"J", {}, {}, false};
    for (std::size_t i = 0; i < tr.loss.size(); ++i) { ls.x.push_back(static_cast<double>(i)); ls.y.push_back(tr.loss[i]); }
    lp.series.push_back(ls);
    write(dir + "/backprop_loss.svg", svgLinePlot(lp));
    return 0;
}
