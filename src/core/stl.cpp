#include "stl.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>

#include "nodal_csv.hpp"

namespace wb {
namespace {

std::vector<std::string> tokens(const std::string& line) {
    std::vector<std::string> t;
    std::istringstream is(line);
    std::string w;
    while (is >> w) t.push_back(w);
    return t;
}

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool parseNumber(const std::string& s, double& out) {
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end != s.c_str() && *end == '\0' && std::isfinite(out);
}

std::string at(std::size_t line, const std::string& msg) { return "Line " + std::to_string(line) + ": " + msg; }

}  // namespace

StlResult parseAsciiStl(const std::string& text, const StlLimits& limits) {
    StlResult r;
    if (text.empty()) {
        r.error = "The file is empty (0 bytes). Choose an ASCII STL file that contains at least one facet.";
        return r;
    }
    if (text.size() > limits.maxBytes) {
        r.error = "File is larger than the supported limit of " + std::to_string(limits.maxBytes / (1024 * 1024)) + " MB.";
        return r;
    }
    if (text.find('\0') != std::string::npos) {
        r.error = "The file contains binary data. Only ASCII STL is supported; re-export the model as ASCII STL.";
        return r;
    }

    std::istringstream in(text);
    std::string line;
    std::size_t ln = 0;
    bool sawSolid = false, sawEnd = false;
    enum State { Idle, InSolid, InFacet, InLoop } st = Idle;
    Triangle tri;
    int vcount = 0;
    bool haveNormal = false;

    while (std::getline(in, line)) {
        ++ln;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (ln == 1 && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF) line.erase(0, 3);  // UTF-8 BOM
        auto t = tokens(line);
        if (t.empty()) continue;
        const std::string kw = lower(t[0]);

        if (kw == "solid") {
            if (st != Idle) { r.error = at(ln, "unexpected 'solid' inside another solid."); return r; }
            sawSolid = true;
            st = InSolid;
            for (std::size_t i = 1; i < t.size(); ++i) r.mesh.name += (i > 1 ? " " : "") + t[i];
        } else if (kw == "endsolid") {
            if (st != InSolid) { r.error = at(ln, "'endsolid' without a complete matching 'solid' block."); return r; }
            sawEnd = true;
            st = Idle;
        } else if (kw == "facet") {
            if (st != InSolid) { r.error = at(ln, "'facet' is not allowed here (missing 'solid' or unclosed previous facet)."); return r; }
            if (t.size() != 5 || lower(t[1]) != "normal") { r.error = at(ln, "expected 'facet normal nx ny nz'."); return r; }
            double v[3];
            for (int i = 0; i < 3; ++i)
                if (!parseNumber(t[2 + i], v[i])) { r.error = at(ln, "invalid or non-finite normal component '" + t[2 + i] + "'."); return r; }
            tri = Triangle{};
            tri.normal = {v[0], v[1], v[2]};
            vcount = 0;
            haveNormal = true;
            st = InFacet;
        } else if (kw == "outer") {
            if (st != InFacet || t.size() != 2 || lower(t[1]) != "loop") { r.error = at(ln, "expected 'outer loop' directly after 'facet normal'."); return r; }
            st = InLoop;
        } else if (kw == "vertex") {
            if (st != InLoop) { r.error = at(ln, "'vertex' outside an 'outer loop'."); return r; }
            if (vcount >= 3) { r.error = at(ln, "a facet has more than 3 vertices; only triangles are supported."); return r; }
            if (t.size() != 4) { r.error = at(ln, "expected 'vertex x y z'."); return r; }
            double v[3];
            for (int i = 0; i < 3; ++i)
                if (!parseNumber(t[1 + i], v[i])) { r.error = at(ln, "invalid or non-finite vertex coordinate '" + t[1 + i] + "'."); return r; }
            tri.v[vcount++] = {v[0], v[1], v[2]};
        } else if (kw == "endloop") {
            if (st != InLoop) { r.error = at(ln, "'endloop' without 'outer loop'."); return r; }
            if (vcount != 3) { r.error = at(ln, "facet has " + std::to_string(vcount) + " vertices, expected exactly 3."); return r; }
            st = InFacet;
        } else if (kw == "endfacet") {
            if (st != InFacet || !haveNormal || vcount != 3) { r.error = at(ln, "'endfacet' without a complete facet."); return r; }
            if (r.mesh.triangles.size() >= limits.maxTriangles) {
                r.error = "More than " + std::to_string(limits.maxTriangles) + " triangles; this exceeds the supported limit.";
                return r;
            }
            r.mesh.triangles.push_back(tri);
            haveNormal = false;
            st = InSolid;
        } else {
            r.error = at(ln, "unrecognised keyword '" + t[0] + "'. Is this really an ASCII STL file?");
            return r;
        }
    }

    if (!sawSolid) { r.error = "No 'solid' header found. This does not look like an ASCII STL file (binary STL is not supported)."; return r; }
    if (!sawEnd || st != Idle) { r.error = "File ended unexpectedly (missing 'endfacet', 'endloop' or 'endsolid'); it may be truncated."; return r; }
    if (r.mesh.triangles.empty()) { r.error = "The solid contains no facets, so there is nothing to display."; return r; }

    const double inf = std::numeric_limits<double>::infinity();
    Vec3 lo{inf, inf, inf}, hi{-inf, -inf, -inf};
    for (const auto& tr : r.mesh.triangles)
        for (const auto& p : tr.v) {
            lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
            hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
        }
    r.mesh.boundsMin = lo;
    r.mesh.boundsMax = hi;
    r.ok = true;
    return r;
}

StlResult loadAsciiStlFile(const std::string& path, const StlLimits& limits) {
    StlResult r;
    std::string text;
    if (!readTextFile(path, limits.maxBytes, text, r.error)) return r;
    return parseAsciiStl(text, limits);
}

}  // namespace wb
