// ASCII STL parser with validation. Units are whatever the file uses (the
// workbench treats STL coordinates as unit-less model units and says so in UI).
#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace wb {

struct Vec3 {
    double x = 0, y = 0, z = 0;
};

struct Triangle {
    Vec3 normal;           // as stored in the file
    std::array<Vec3, 3> v;
};

struct StlMesh {
    std::string name;
    std::vector<Triangle> triangles;
    Vec3 boundsMin, boundsMax;
};

struct StlLimits {
    std::size_t maxBytes = 200u * 1024u * 1024u;   // 200 MB
    std::size_t maxTriangles = 2000000;
};

struct StlResult {
    bool ok = false;
    std::string error;     // actionable, includes line number when relevant
    StlMesh mesh;
};

StlResult parseAsciiStl(const std::string& text, const StlLimits& limits = {});
StlResult loadAsciiStlFile(const std::string& path, const StlLimits& limits = {});

}  // namespace wb
