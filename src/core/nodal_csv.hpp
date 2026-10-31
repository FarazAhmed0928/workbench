// Parser for CSV exports of nodal temperatures:
//   node_id,x_m,y_m,z_m,temperature_C
// Lines starting with '#' (before or after the header) are comments.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wb {

struct NodalPoint {
    std::int64_t id = 0;
    double x = 0, y = 0, z = 0;   // metres
    double temperature = 0;       // degC
};

struct NodalCloud {
    std::vector<NodalPoint> points;
    double minT = 0, maxT = 0;
    double boundsMin[3] = {0, 0, 0}, boundsMax[3] = {0, 0, 0};
};

struct CsvLimits {
    std::size_t maxBytes = 200u * 1024u * 1024u;
    std::size_t maxRows = 5000000;
};

struct CsvResult {
    bool ok = false;
    std::string error;
    NodalCloud cloud;
};

CsvResult parseNodalCsv(const std::string& text, const CsvLimits& limits = {});
CsvResult loadNodalCsvFile(const std::string& path, const CsvLimits& limits = {});

// Shared helper: read a whole file, with clear errors.
bool readTextFile(const std::string& path, std::size_t maxBytes, std::string& out, std::string& error);

}  // namespace wb
