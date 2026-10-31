#include <cmath>
#include <cstdio>
#include <string>

#include "nodal_csv.hpp"
#include "stl.hpp"

static int g_fail = 0;
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++g_fail;                                                   \
        }                                                               \
    } while (0)

static const char* kGoodStl =
    "solid t\n facet normal 0 0 1\n  outer loop\n   vertex 0 0 0\n   vertex 1 0 0\n   vertex 0 1 0\n  endloop\n endfacet\nendsolid t\n";

static bool contains(const std::string& s, const char* sub) { return s.find(sub) != std::string::npos; }

int main() {
    using namespace wb;

    // ---- STL positive ----
    auto a = parseAsciiStl(kGoodStl);
    CHECK(a.ok);
    CHECK(a.mesh.triangles.size() == 1);
    CHECK(a.mesh.boundsMax.x == 1.0 && a.mesh.boundsMin.z == 0.0);
    auto crlf = parseAsciiStl(std::string("solid t\r\n facet normal 0 0 1\r\n outer loop\r\n vertex 0 0 0\r\n vertex 1 0 0\r\n vertex 0 1 0\r\n endloop\r\n endfacet\r\nendsolid t\r\n"));
    CHECK(crlf.ok);
    auto file = loadAsciiStlFile("data/example_rod.stl");
    CHECK(file.ok);
    CHECK(file.mesh.triangles.size() == 96);
    CHECK(std::fabs(file.mesh.boundsMax.x - 1.0) < 1e-9);

    // ---- STL negative ----
    auto e = parseAsciiStl("");
    CHECK(!e.ok && contains(e.error, "empty"));
    CHECK(!parseAsciiStl("solid t\nendsolid t\n").ok);                       // no facets
    CHECK(!parseAsciiStl("hello world\n").ok);                                // garbage
    CHECK(!parseAsciiStl(std::string("solid\0abc", 9)).ok);                   // binary
    std::string nan = kGoodStl;
    nan.replace(nan.find("vertex 1 0 0"), 12, "vertex nan 0 0");
    auto rn = parseAsciiStl(nan);
    CHECK(!rn.ok && contains(rn.error, "non-finite") && contains(rn.error, "Line 5"));
    std::string inf = kGoodStl;
    inf.replace(inf.find("vertex 1 0 0"), 12, "vertex inf 0 0");
    CHECK(!parseAsciiStl(inf).ok);
    std::string trunc = std::string(kGoodStl).substr(0, 70);
    CHECK(!parseAsciiStl(trunc).ok);
    std::string twoV = "solid t\n facet normal 0 0 1\n outer loop\n vertex 0 0 0\n vertex 1 0 0\n endloop\n endfacet\nendsolid\n";
    CHECK(!parseAsciiStl(twoV).ok);
    StlLimits tiny;
    tiny.maxBytes = 10;
    CHECK(!parseAsciiStl(kGoodStl, tiny).ok);
    CHECK(!loadAsciiStlFile("data/does_not_exist.stl").ok);

    // ---- CSV positive ----
    auto c = parseNodalCsv("node_id,x_m,y_m,z_m,temperature_C\n1,0,0,0,100\n2,1,0,0,20\n");
    CHECK(c.ok && c.cloud.points.size() == 2);
    CHECK(c.cloud.minT == 20 && c.cloud.maxT == 100 && c.cloud.boundsMax[0] == 1);
    auto cf = loadNodalCsvFile("data/example_nodal_temperature_SYNTHETIC.csv");
    CHECK(cf.ok && cf.cloud.points.size() == 273);
    CHECK(std::fabs(cf.cloud.maxT - 188.5) < 1e-9);
    CHECK(parseNodalCsv("node_id,x_m,y_m,z_m,temperature_C\r\n1,0,0,0,5\r\n").ok);

    // ---- CSV negative ----
    CHECK(!parseNodalCsv("").ok);
    auto h = parseNodalCsv("node_id,x_m,y_m,z_m,temperature_C\n");
    CHECK(!h.ok && contains(h.error, "no data rows"));
    CHECK(!parseNodalCsv("id,x,y,z,T\n1,0,0,0,1\n").ok);                       // wrong header
    CHECK(!parseNodalCsv("node_id,x_m,y_m,z_m,temperature_C\n1,0,0,0\n").ok);  // too few columns
    auto bn = parseNodalCsv("node_id,x_m,y_m,z_m,temperature_C\n1,0,0,0,nan\n");
    CHECK(!bn.ok && contains(bn.error, "Line 2") && contains(bn.error, "temperature_C"));
    CHECK(!parseNodalCsv("node_id,x_m,y_m,z_m,temperature_C\n1,inf,0,0,5\n").ok);
    CHECK(!parseNodalCsv("node_id,x_m,y_m,z_m,temperature_C\n1,abc,0,0,5\n").ok);
    CHECK(!parseNodalCsv("node_id,x_m,y_m,z_m,temperature_C\n1.5,0,0,0,5\n").ok);  // non-integer id
    CHECK(!parseNodalCsv("node_id,x_m,y_m,z_m,temperature_C\n1,0,0,0,5\n1,1,0,0,6\n").ok);  // duplicate
    CsvLimits lim;
    lim.maxRows = 1;
    CHECK(!parseNodalCsv("node_id,x_m,y_m,z_m,temperature_C\n1,0,0,0,5\n2,1,0,0,6\n", lim).ok);
    CHECK(!loadNodalCsvFile("data/does_not_exist.csv").ok);

    if (g_fail == 0) std::printf("test_parsers: all checks passed\n");
    return g_fail == 0 ? 0 : 1;
}
