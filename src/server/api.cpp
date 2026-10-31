#include "api.hpp"

#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <thread>

#include "backprop.hpp"
#include "batch.hpp"
#include "benchmark.hpp"
#include "devices.hpp"
#include "fea1d.hpp"
#include "nodal_csv.hpp"
#include "stl.hpp"
#include "sysinfo.hpp"
#include "json.hpp"

namespace wb {
using json = nlohmann::json;

namespace {

long long nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void fail(httplib::Response& res, int code, const std::string& msg) {
    res.status = code;
    res.set_content(json{{"error", msg}}.dump(), "application/json");
}
void ok(httplib::Response& res, const json& j) { res.set_content(j.dump(), "application/json"); }

// Read a numeric field with a default; reports a precise message if it is not a number.
bool num(const json& j, const char* key, double& out, std::string& err) {
    if (!j.contains(key)) return true;
    if (!j[key].is_number()) { err = std::string("Field '") + key + "' must be a number."; return false; }
    out = j[key].get<double>();
    return true;
}

bool parseBody(const httplib::Request& req, json& j, std::string& err) {
    if (req.body.empty()) { j = json::object(); return true; }
    j = json::parse(req.body, nullptr, false);
    if (j.is_discarded() || !j.is_object()) { err = "Request body is not valid JSON."; return false; }
    return true;
}

bool rodFromJson(const json& j, RodProblem& p, int& N, std::string& err) {
    double n = 10;
    if (!num(j, "L", p.length, err) || !num(j, "A", p.area, err) || !num(j, "k", p.k, err) || !num(j, "q", p.q, err) ||
        !num(j, "T0", p.tLeft, err) || !num(j, "TL", p.tRight, err) || !num(j, "N", n, err))
        return false;
    if (!std::isfinite(n) || n != std::floor(n) || n < 1 || n > 1e6) { err = "Number of elements N must be an integer between 1 and 1,000,000."; return false; }
    N = static_cast<int>(n);
    err = validate(p, N);
    return err.empty();
}

json stlToJson(const StlMesh& m) {
    std::vector<float> pos;
    pos.reserve(m.triangles.size() * 9);
    for (const auto& t : m.triangles)
        for (const auto& v : t.v) { pos.push_back(static_cast<float>(v.x)); pos.push_back(static_cast<float>(v.y)); pos.push_back(static_cast<float>(v.z)); }
    return {{"name", m.name}, {"triangles", m.triangles.size()}, {"positions", pos},
            {"boundsMin", {m.boundsMin.x, m.boundsMin.y, m.boundsMin.z}}, {"boundsMax", {m.boundsMax.x, m.boundsMax.y, m.boundsMax.z}}};
}

json cloudToJson(const NodalCloud& c) {
    std::vector<double> x, y, z, t;
    std::vector<long long> id;
    for (const auto& p : c.points) { id.push_back(p.id); x.push_back(p.x); y.push_back(p.y); z.push_back(p.z); t.push_back(p.temperature); }
    return {{"count", c.points.size()}, {"ids", id}, {"x", x}, {"y", y}, {"z", z}, {"T", t}, {"minT", c.minT}, {"maxT", c.maxT},
            {"boundsMin", {c.boundsMin[0], c.boundsMin[1], c.boundsMin[2]}}, {"boundsMax", {c.boundsMax[0], c.boundsMax[1], c.boundsMax[2]}},
            {"units", {{"length", "m"}, {"temperature", "degC"}}}};
}

json systemToJson(const SystemInfo& s) {
    return {{"cpu", s.cpuModel}, {"physicalCores", s.physicalCores}, {"logicalCores", s.logicalCores}, {"ramGiB", s.ramGiB},
            {"os", s.os}, {"compiler", s.compiler}, {"flags", s.flags}};
}

json benchToJson(const BenchReport& r) {
    json rows = json::array();
    for (const auto& w : r.rows)
        rows.push_back({{"workers", w.workers}, {"medianSeconds", w.medianSeconds}, {"runsSeconds", w.runsSeconds}, {"speedup", w.speedup},
                        {"efficiency", w.efficiency}, {"maxDiff", w.maxDiffVsSerial}});
    return {{"system", systemToJson(r.system)}, {"cases", r.config.cases}, {"elements", r.config.elements}, {"seed", r.config.seed},
            {"warmups", r.warmups}, {"repetitions", r.repetitions}, {"rows", rows}, {"agreementOk", r.agreementOk},
            {"speedupAssessable", r.speedupAssessable}, {"note", r.note}, {"csv", benchCsv(r)}, {"text", benchText(r)}};
}

json deviceToJson(const DeviceInfo& d) {
    return {{"id", d.id}, {"type", toString(d.type)}, {"name", d.name}, {"role", d.role}, {"capabilities", d.capabilities},
            {"state", toString(d.state)}, {"simulated", d.simulated}};
}
json statusToJson(const DeviceStatus& s) {
    return {{"ok", s.ok}, {"error", toString(s.error)}, {"message", s.message}, {"attempts", s.attempts}, {"simulated", true}};
}

}  // namespace

struct ApiServer::Impl {
    ServerConfig cfg;
    MockDeviceManager devices;

    // ---- background performance job ----
    std::mutex m;
    std::string state = "idle";     // idle | running | done | cancelled | error
    double progress = 0;
    std::string label;
    json result;
    std::string error;
    BatchControl ctl;
    std::thread worker;

    ~Impl() {
        ctl.cancel = true;
        if (worker.joinable()) worker.join();
    }
};

ApiServer::ApiServer(ServerConfig cfg) : impl_(new Impl) {
    impl_->cfg = std::move(cfg);
    lastPingMs_ = nowMs();
}
ApiServer::~ApiServer() { delete impl_; }

long long ApiServer::msSinceLastPing() const { return nowMs() - lastPingMs_.load(); }

void ApiServer::registerRoutes(httplib::Server& svr) {
    Impl& S = *impl_;

    svr.Get("/api/ping", [this](const httplib::Request&, httplib::Response& res) {
        lastPingMs_ = nowMs();
        everPinged_ = true;
        ok(res, {{"ok", true}});
    });
    svr.Post("/api/shutdown", [this](const httplib::Request&, httplib::Response& res) {
        quit_ = true;
        ok(res, {{"ok", true}});
    });
    svr.Get("/api/info", [](const httplib::Request&, httplib::Response& res) {
        ok(res, {{"name", "Unified Engineering Workbench"}, {"version", "0.1.0"}, {"system", systemToJson(querySystemInfo())},
                 {"limits", {{"maxFileMB", 50}, {"maxTriangles", 500000}, {"maxCsvRows", 1000000}}}});
    });

    // ---------------- Task C: thermal FEA ----------------
    svr.Post("/api/fea/solve", [](const httplib::Request& req, httplib::Response& res) {
        json j;
        std::string err;
        RodProblem p;
        int N = 10;
        if (!parseBody(req, j, err) || !rodFromJson(j, p, N, err)) return fail(res, 400, err);
        auto s = solveRod(p, N);
        auto e = computeErrors(p, s);
        std::vector<double> te;
        for (double x : s.x) te.push_back(exactTemperature(p, x));
        ok(res, {{"N", N}, {"x", s.x}, {"T", s.T}, {"Texact", te}, {"h", e.h}, {"maxNodalError", e.maxNodalError}, {"maxMidpointError", e.maxMidpointError}});
    });
    svr.Post("/api/fea/convergence", [](const httplib::Request& req, httplib::Response& res) {
        json j;
        std::string err;
        RodProblem p;
        int N = 10;
        if (!parseBody(req, j, err) || !rodFromJson(j, p, N, err)) return fail(res, 400, err);
        const std::vector<int> meshes = {10, 20, 40, 80};
        auto rows = convergenceStudy(p, meshes);
        json out = json::array(), sols = json::array();
        for (size_t i = 0; i < rows.size(); ++i) {
            out.push_back({{"N", rows[i].err.elements}, {"h", rows[i].err.h}, {"maxNodalError", rows[i].err.maxNodalError},
                           {"maxMidpointError", rows[i].err.maxMidpointError}, {"observedOrder", rows[i].observedOrder}});
            auto s = solveRod(p, meshes[i]);
            sols.push_back({{"N", meshes[i]}, {"x", s.x}, {"T", s.T}});
        }
        std::vector<double> xd, td;
        for (int i = 0; i <= 200; ++i) { xd.push_back(p.length * i / 200.0); td.push_back(exactTemperature(p, xd.back())); }
        ok(res, {{"rows", out}, {"solutions", sols}, {"exact", {{"x", xd}, {"T", td}}}, {"csv", convergenceCsv(rows)}});
    });
    svr.Post("/api/fea/export", [](const httplib::Request& req, httplib::Response& res) {
        json j;
        std::string err;
        RodProblem p;
        int N = 10;
        if (!parseBody(req, j, err) || !rodFromJson(j, p, N, err)) return fail(res, 400, err);
        std::string kind = j.value("kind", "solution");
        std::string csv = kind == "convergence" ? convergenceCsv(convergenceStudy(p, {10, 20, 40, 80})) : solutionCsv(p, solveRod(p, N));
        res.set_header("Content-Disposition", "attachment; filename=\"fea_" + kind + ".csv\"");
        res.set_content(csv, "text/csv");
    });

    // ---------------- Task B: geometry / results ----------------
    StlLimits stlLim;
    stlLim.maxBytes = 50u * 1024 * 1024;
    stlLim.maxTriangles = 500000;
    CsvLimits csvLim;
    csvLim.maxBytes = 50u * 1024 * 1024;
    csvLim.maxRows = 1000000;

    svr.Post("/api/geometry/stl", [stlLim](const httplib::Request& req, httplib::Response& res) {
        auto r = parseAsciiStl(req.body, stlLim);
        if (!r.ok) return fail(res, 400, r.error);
        ok(res, stlToJson(r.mesh));
    });
    svr.Post("/api/results/csv", [csvLim](const httplib::Request& req, httplib::Response& res) {
        auto r = parseNodalCsv(req.body, csvLim);
        if (!r.ok) return fail(res, 400, r.error);
        ok(res, cloudToJson(r.cloud));
    });
    svr.Get("/api/examples/stl", [&S, stlLim](const httplib::Request&, httplib::Response& res) {
        auto r = loadAsciiStlFile(S.cfg.dataDir + "/example_rod.stl", stlLim);
        if (!r.ok) return fail(res, 500, "Bundled example is missing: " + r.error);
        auto j = stlToJson(r.mesh);
        j["source"] = "example_rod.stl (synthetic, self-created)";
        ok(res, j);
    });
    svr.Get("/api/examples/csv", [&S, csvLim](const httplib::Request&, httplib::Response& res) {
        auto r = loadNodalCsvFile(S.cfg.dataDir + "/example_nodal_temperature_SYNTHETIC.csv", csvLim);
        if (!r.ok) return fail(res, 500, "Bundled example is missing: " + r.error);
        auto j = cloudToJson(r.cloud);
        j["source"] = "example_nodal_temperature_SYNTHETIC.csv (SYNTHETIC, not an ANSYS export)";
        ok(res, j);
    });

    // ---------------- Task D: performance (background job) ----------------
    svr.Post("/api/perf/start", [&S](const httplib::Request& req, httplib::Response& res) {
        json j;
        std::string err;
        if (!parseBody(req, j, err)) return fail(res, 400, err);
        double cases = 20000, elements = 2000, seed = 12345, workers = 4;
        if (!num(j, "cases", cases, err) || !num(j, "elements", elements, err) || !num(j, "seed", seed, err) || !num(j, "maxWorkers", workers, err))
            return fail(res, 400, err);
        BatchConfig cfg;
        if (cases != std::floor(cases) || elements != std::floor(elements) || seed < 0 || seed != std::floor(seed) || workers < 1 || workers > 64)
            return fail(res, 400, "cases, elements and seed must be whole numbers (seed >= 0) and maxWorkers between 1 and 64.");
        cfg.cases = static_cast<int>(cases);
        cfg.elements = static_cast<int>(elements);
        cfg.seed = static_cast<std::uint64_t>(seed);
        err = validate(cfg);
        if (!err.empty()) return fail(res, 400, err);
        if (static_cast<double>(cfg.cases) * cfg.elements > 4e8)
            return fail(res, 400, "Workload too large (cases x elements > 4e8). Reduce it so the run finishes in reasonable time.");

        std::lock_guard<std::mutex> lk(S.m);
        if (S.state == "running") return fail(res, 409, "A benchmark is already running. Cancel it or wait for it to finish.");
        if (S.worker.joinable()) S.worker.join();
        S.state = "running";
        S.progress = 0;
        S.label = "starting";
        S.result = json();
        S.error.clear();
        S.ctl.cancel = false;
        S.ctl.done = 0;
        const int maxWorkers = static_cast<int>(workers);
        S.worker = std::thread([&S, cfg, maxWorkers] {
            try {
                auto rep = runBenchmark(cfg, maxWorkers, 1, 5, &S.ctl, [&S](const std::string& l, double f) {
                    std::lock_guard<std::mutex> lk2(S.m);
                    S.label = l;
                    S.progress = f;
                });
                std::lock_guard<std::mutex> lk2(S.m);
                if (rep.cancelled) { S.state = "cancelled"; S.label = "cancelled; no partial results kept"; }
                else { S.state = "done"; S.progress = 1; S.result = benchToJson(rep); }
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lk2(S.m);
                S.state = "error";
                S.error = e.what();
            }
        });
        ok(res, {{"state", "running"}});
    });
    svr.Get("/api/perf/status", [&S](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lk(S.m);
        json j = {{"state", S.state}, {"progress", S.progress}, {"label", S.label}, {"error", S.error}};
        if (S.state == "done") j["result"] = S.result;
        ok(res, j);
    });
    svr.Post("/api/perf/cancel", [&S](const httplib::Request&, httplib::Response& res) {
        S.ctl.cancel = true;
        ok(res, {{"ok", true}});
    });

    // ---------------- Task E: simulated hardware ----------------
    svr.Get("/api/devices", [&S](const httplib::Request&, httplib::Response& res) {
        json arr = json::array();
        for (const auto& d : S.devices.discover()) arr.push_back(deviceToJson(d));
        ok(res, {{"simulated", true}, {"devices", arr}});
    });
    svr.Post(R"(/api/devices/([^/]+)/(connect|disconnect|telemetry|connect-retry|inject-timeout))",
             [&S](const httplib::Request& req, httplib::Response& res) {
                 const std::string id = req.matches[1], action = req.matches[2];
                 DeviceStatus st;
                 json out;
                 if (action == "connect") st = S.devices.connect(id);
                 else if (action == "disconnect") st = S.devices.disconnect(id);
                 else if (action == "connect-retry") st = S.devices.connectWithRetry(id, 3);
                 else if (action == "inject-timeout") {
                     std::vector<std::string> caps;
                     st = S.devices.capabilities(id, caps);
                     if (st.ok) { S.devices.injectTimeouts(id, 1); st.message = "Injected one simulated timeout for the next call to " + id + "."; }
                 } else {
                     Telemetry t;
                     st = S.devices.readTelemetry(id, t);
                     if (st.ok) {
                         json vals = json::array();
                         for (const auto& v : t.values) vals.push_back({{"name", v.name}, {"value", v.value}, {"unit", v.unit}});
                         out["telemetry"] = {{"deviceId", t.deviceId}, {"sequence", t.sequence}, {"values", vals}, {"simulated", true}};
                     }
                 }
                 out["status"] = statusToJson(st);
                 for (const auto& d : S.devices.discover()) if (d.id == id) out["device"] = deviceToJson(d);
                 if (!st.ok && st.error == DeviceError::NotFound) res.status = 404;
                 ok(res, out);
             });

    // ---------------- Task F: backpropagation ----------------
    svr.Get("/api/backprop", [](const httplib::Request&, httplib::Response& res) {
        BpParams p0;
        const double x = 1.0, y = 2.0, lr = 0.1;
        auto d = backward(p0, x, y);
        auto n = numericGradient(p0, x, y, 1e-6);
        auto p1 = gradientStep(p0, d.g, lr);
        auto tr = train(p0, x, y, lr, 100);
        ok(res, {{"x", x}, {"y", y}, {"lr", lr}, {"initial", {{"w", p0.w}, {"b", p0.b}, {"v", p0.v}, {"c", p0.c}}},
                 {"forward", {{"z", d.f.z}, {"h", d.f.h}, {"yhat", d.f.yhat}, {"J", d.f.loss}}},
                 {"chain", {{"dJ_dyhat", d.e}, {"dJ_dh", d.dJ_dh}, {"dh_dz", d.dh_dz}, {"dJ_dz", d.dJ_dz}}},
                 {"grad", {{"w", d.g.dw}, {"b", d.g.db}, {"v", d.g.dv}, {"c", d.g.dc}}},
                 {"numericGrad", {{"w", n.dw}, {"b", n.db}, {"v", n.dv}, {"c", n.dc}}},
                 {"maxGradError", maxAbsGradError(d.g, n)}, {"gradCheckPass", maxAbsGradError(d.g, n) <= 1e-5},
                 {"updated", {{"w", p1.w}, {"b", p1.b}, {"v", p1.v}, {"c", p1.c}}}, {"lossAfterUpdate", forward(p1, x, y).loss},
                 {"loss", tr.loss}, {"csv", lossCsv(tr)}});
    });
}

}  // namespace wb
