#include "nodal_csv.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace wb {
namespace {

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> f;
    std::string cur;
    for (char c : line) {
        if (c == ',') { f.push_back(trim(cur)); cur.clear(); }
        else cur += c;
    }
    f.push_back(trim(cur));
    return f;
}

bool toDouble(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end != s.c_str() && *end == '\0' && std::isfinite(out);
}

bool toInt(const std::string& s, std::int64_t& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    long long v = std::strtoll(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0' || errno == ERANGE) return false;
    out = v;
    return true;
}

std::string at(std::size_t line, const std::string& msg) { return "Line " + std::to_string(line) + ": " + msg; }

}  // namespace

bool readTextFile(const std::string& path, std::size_t maxBytes, std::string& out, std::string& error) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { error = "Cannot open '" + path + "'. Check that the file exists and is not in use."; return false; }
    const std::streamoff size = f.tellg();
    if (size < 0) { error = "Cannot determine the size of '" + path + "'."; return false; }
    if (static_cast<std::size_t>(size) > maxBytes) {
        error = "File is larger than the supported limit of " + std::to_string(maxBytes / (1024 * 1024)) + " MB.";
        return false;
    }
    f.seekg(0);
    out.resize(static_cast<std::size_t>(size));
    if (size > 0 && !f.read(&out[0], size)) { error = "Failed while reading '" + path + "'."; return false; }
    return true;
}

CsvResult parseNodalCsv(const std::string& text, const CsvLimits& limits) {
    CsvResult r;
    if (text.empty()) {
        r.error = "The file is empty (0 bytes). Expected a header 'node_id,x_m,y_m,z_m,temperature_C' followed by data rows.";
        return r;
    }
    if (text.size() > limits.maxBytes) {
        r.error = "File is larger than the supported limit of " + std::to_string(limits.maxBytes / (1024 * 1024)) + " MB.";
        return r;
    }

    std::istringstream in(text);
    std::string line;
    std::size_t ln = 0;
    bool haveHeader = false;
    std::unordered_set<std::int64_t> seen;

    while (std::getline(in, line)) {
        ++ln;
        if (ln == 1 && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF) line.erase(0, 3);
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        auto f = split(t);

        if (!haveHeader) {
            const std::vector<std::string> want = {"node_id", "x_m", "y_m", "z_m", "temperature_C"};
            if (f != want) {
                r.error = at(ln, "header must be exactly 'node_id,x_m,y_m,z_m,temperature_C' (found '" + t + "').");
                return r;
            }
            haveHeader = true;
            continue;
        }
        if (f.size() != 5) {
            r.error = at(ln, "expected 5 columns but found " + std::to_string(f.size()) + ".");
            return r;
        }
        if (r.cloud.points.size() >= limits.maxRows) {
            r.error = "More than " + std::to_string(limits.maxRows) + " rows; this exceeds the supported limit.";
            return r;
        }
        NodalPoint p;
        if (!toInt(f[0], p.id)) { r.error = at(ln, "node_id '" + f[0] + "' is not an integer."); return r; }
        double* dst[4] = {&p.x, &p.y, &p.z, &p.temperature};
        const char* names[4] = {"x_m", "y_m", "z_m", "temperature_C"};
        for (int i = 0; i < 4; ++i)
            if (!toDouble(f[1 + i], *dst[i])) {
                r.error = at(ln, std::string("value '") + f[1 + i] + "' in column " + names[i] + " is not a finite number.");
                return r;
            }
        if (!seen.insert(p.id).second) { r.error = at(ln, "duplicate node_id " + std::to_string(p.id) + "."); return r; }
        r.cloud.points.push_back(p);
    }

    if (!haveHeader) { r.error = "No header row found (only blank or comment lines)."; return r; }
    if (r.cloud.points.empty()) { r.error = "The file has a header but no data rows, so there is nothing to display."; return r; }

    auto& c = r.cloud;
    c.minT = c.maxT = c.points[0].temperature;
    for (int i = 0; i < 3; ++i) c.boundsMin[i] = c.boundsMax[i] = (i == 0 ? c.points[0].x : i == 1 ? c.points[0].y : c.points[0].z);
    for (const auto& p : c.points) {
        c.minT = std::min(c.minT, p.temperature);
        c.maxT = std::max(c.maxT, p.temperature);
        const double v[3] = {p.x, p.y, p.z};
        for (int i = 0; i < 3; ++i) {
            c.boundsMin[i] = std::min(c.boundsMin[i], v[i]);
            c.boundsMax[i] = std::max(c.boundsMax[i], v[i]);
        }
    }
    r.ok = true;
    return r;
}

CsvResult loadNodalCsvFile(const std::string& path, const CsvLimits& limits) {
    CsvResult r;
    std::string text;
    if (!readTextFile(path, limits.maxBytes, text, r.error)) return r;
    return parseNodalCsv(text, limits);
}

}  // namespace wb
