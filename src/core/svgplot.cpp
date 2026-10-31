#include "svgplot.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace wb {
namespace {

std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '&') o += "&amp;";
        else if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else o += c;
    }
    return o;
}

std::string num(double v) {
    std::ostringstream o;
    const double a = std::fabs(v);
    if (a != 0 && (a < 1e-3 || a >= 1e5)) o << std::scientific << std::setprecision(1) << v;
    else o << std::setprecision(4) << v;
    return o.str();
}

double niceStep(double range, int target) {
    const double raw = range / target;
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    const double f = raw / mag;
    return (f < 1.5 ? 1 : f < 3 ? 2 : f < 7 ? 5 : 10) * mag;
}

}  // namespace

std::string svgLinePlot(const PlotSpec& s, int W, int H) {
    const double ml = 80, mr = 20, mt = 40, mb = 60;
    double xmin = 1e300, xmax = -1e300, ymin = 1e300, ymax = -1e300;
    for (const auto& se : s.series)
        for (std::size_t i = 0; i < se.x.size(); ++i) {
            if ((s.logX && se.x[i] <= 0) || (s.logY && se.y[i] <= 0)) continue;
            xmin = std::min(xmin, se.x[i]); xmax = std::max(xmax, se.x[i]);
            ymin = std::min(ymin, se.y[i]); ymax = std::max(ymax, se.y[i]);
        }
    if (xmin >= xmax) { xmin -= 1; xmax += 1; }
    if (ymin >= ymax) { ymin -= 1; ymax += 1; }
    auto tx = [&](double v) { return s.logX ? std::log10(v) : v; };
    auto ty = [&](double v) { return s.logY ? std::log10(v) : v; };
    double X0 = tx(xmin), X1 = tx(xmax), Y0 = ty(ymin), Y1 = ty(ymax);
    if (s.logY) { Y0 = std::floor(Y0); Y1 = std::ceil(Y1); }
    else { const double pad = 0.05 * (Y1 - Y0); Y0 -= pad; Y1 += pad; }
    if (s.logX) { X0 = std::floor(X0 * 4) / 4; X1 = std::ceil(X1 * 4) / 4; }
    auto px = [&](double v) { return ml + (tx(v) - X0) / (X1 - X0) * (W - ml - mr); };
    auto py = [&](double v) { return H - mb - (ty(v) - Y0) / (Y1 - Y0) * (H - mt - mb); };

    static const char* colors[] = {"#1f77b4", "#d62728", "#2ca02c", "#9467bd", "#ff7f0e", "#17becf"};
    std::ostringstream o;
    o << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << W << "\" height=\"" << H << "\" viewBox=\"0 0 " << W << ' ' << H
      << "\" font-family=\"sans-serif\" font-size=\"12\">\n<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n"
      << "<text x=\"" << W / 2 << "\" y=\"22\" text-anchor=\"middle\" font-size=\"15\" font-weight=\"bold\">" << esc(s.title) << "</text>\n";

    // grid + ticks
    auto ticks = [&](double a, double b, bool log) {
        std::vector<double> t;
        if (log) { for (double e = std::ceil(a); e <= b + 1e-9; e += std::max(1.0, std::floor((b - a) / 6))) t.push_back(e); }
        else { const double st = niceStep(b - a, 6); for (double v = std::ceil(a / st) * st; v <= b + 1e-9 * st; v += st) t.push_back(v); }
        return t;
    };
    for (double t : ticks(Y0, Y1, s.logY)) {
        const double yy = H - mb - (t - Y0) / (Y1 - Y0) * (H - mt - mb);
        o << "<line x1=\"" << ml << "\" x2=\"" << W - mr << "\" y1=\"" << yy << "\" y2=\"" << yy << "\" stroke=\"#ddd\"/>\n"
          << "<text x=\"" << ml - 6 << "\" y=\"" << yy + 4 << "\" text-anchor=\"end\">" << num(s.logY ? std::pow(10.0, t) : t) << "</text>\n";
    }
    for (double t : ticks(X0, X1, s.logX)) {
        const double xx = ml + (t - X0) / (X1 - X0) * (W - ml - mr);
        o << "<line x1=\"" << xx << "\" x2=\"" << xx << "\" y1=\"" << mt << "\" y2=\"" << H - mb << "\" stroke=\"#eee\"/>\n"
          << "<text x=\"" << xx << "\" y=\"" << H - mb + 16 << "\" text-anchor=\"middle\">" << num(s.logX ? std::pow(10.0, t) : t) << "</text>\n";
    }
    o << "<rect x=\"" << ml << "\" y=\"" << mt << "\" width=\"" << W - ml - mr << "\" height=\"" << H - mt - mb << "\" fill=\"none\" stroke=\"#333\"/>\n"
      << "<text x=\"" << (ml + W - mr) / 2 << "\" y=\"" << H - 14 << "\" text-anchor=\"middle\">" << esc(s.xLabel) << "</text>\n"
      << "<text transform=\"translate(18," << (mt + H - mb) / 2 << ") rotate(-90)\" text-anchor=\"middle\">" << esc(s.yLabel) << "</text>\n";

    int idx = 0;
    for (const auto& se : s.series) {
        const char* col = colors[idx % 6];
        std::ostringstream pts;
        for (std::size_t i = 0; i < se.x.size(); ++i) {
            if ((s.logX && se.x[i] <= 0) || (s.logY && se.y[i] <= 0)) continue;
            pts << std::fixed << std::setprecision(2) << px(se.x[i]) << ',' << py(se.y[i]) << ' ';
        }
        o << "<polyline fill=\"none\" stroke=\"" << col << "\" stroke-width=\"2\" points=\"" << pts.str() << "\"/>\n";
        if (se.markers)
            for (std::size_t i = 0; i < se.x.size(); ++i)
                if (!((s.logX && se.x[i] <= 0) || (s.logY && se.y[i] <= 0)))
                    o << "<circle cx=\"" << px(se.x[i]) << "\" cy=\"" << py(se.y[i]) << "\" r=\"3.5\" fill=\"" << col << "\"/>\n";
        const double ly = mt + 16 + 16 * idx;
        o << "<line x1=\"" << W - mr - 150 << "\" x2=\"" << W - mr - 126 << "\" y1=\"" << ly - 4 << "\" y2=\"" << ly - 4 << "\" stroke=\"" << col
          << "\" stroke-width=\"2\"/><text x=\"" << W - mr - 120 << "\" y=\"" << ly << "\">" << esc(se.label) << "</text>\n";
        ++idx;
    }
    o << "</svg>\n";
    return o.str();
}

}  // namespace wb
