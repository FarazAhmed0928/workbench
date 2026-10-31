// Minimal dependency-free SVG line plots for the results/ folder.
#pragma once
#include <string>
#include <vector>

namespace wb {

struct PlotSeries {
    std::string label;
    std::vector<double> x, y;
    bool markers = false;
};

struct PlotSpec {
    std::string title, xLabel, yLabel;   // labels include units
    bool logY = false;
    bool logX = false;
    std::vector<PlotSeries> series;
};

std::string svgLinePlot(const PlotSpec& spec, int width = 760, int height = 460);

}  // namespace wb
