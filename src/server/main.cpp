// Launch.exe: starts the local HTTP server (127.0.0.1 only), opens the default
// browser at it, and exits when the browser tab has gone away.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "api.hpp"

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace {

std::string exeDir() {
#if defined(_WIN32)
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string p(buf, n);
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t sz = sizeof(buf);
    std::string p = _NSGetExecutablePath(buf, &sz) == 0 ? buf : ".";
#else
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    std::string p = n > 0 ? std::string(buf, n) : ".";
#endif
    auto pos = p.find_last_of("/\\");
    return pos == std::string::npos ? "." : p.substr(0, pos);
}

bool exists(const std::string& path) { return std::ifstream(path).good(); }

// Find a folder that contains `marker`, looking next to the exe first (packaged layout), then in dev layouts.
std::string findDir(const std::string& name, const std::string& marker) {
    const std::string base = exeDir();
    for (const std::string& c : {base + "/" + name, base + "/../" + name, base + "/../../" + name, "./" + name})
        if (exists(c + "/" + marker)) return c;
    return "";
}

void fatal(const std::string& msg) {
#if defined(_WIN32)
    MessageBoxA(nullptr, msg.c_str(), "Unified Engineering Workbench", MB_OK | MB_ICONERROR);
#else
    fprintf(stderr, "%s\n", msg.c_str());
#endif
}

void openBrowser(const std::string& url) {
#if defined(_WIN32)
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    std::system(("open '" + url + "' >/dev/null 2>&1").c_str());
#else
    std::system(("xdg-open '" + url + "' >/dev/null 2>&1 &").c_str());
#endif
}

int run(bool noBrowser, int fixedPort) {
    const std::string web = findDir("web", "index.html");
    const std::string data = findDir("data", "example_rod.stl");
    if (web.empty()) { fatal("The 'web' folder was not found next to Launch.exe. Extract the whole ZIP before running."); return 2; }
    if (data.empty()) { fatal("The 'data' folder was not found next to Launch.exe. Extract the whole ZIP before running."); return 2; }

    wb::ApiServer api({data, web});
    api.markProcessStart();
    httplib::Server svr;
    svr.set_payload_max_length(256u * 1024 * 1024);
    svr.set_mount_point("/", web);
    api.registerRoutes(svr);

    const int port = fixedPort > 0 ? (svr.bind_to_port("127.0.0.1", fixedPort) ? fixedPort : -1)
                                   : svr.bind_to_any_port("127.0.0.1");  // loopback only: no firewall prompt, not reachable from the network
    if (port < 0) { fatal("Could not open a local port. Another program may be blocking local connections."); return 3; }
    api.markServerReady();
    const std::string url = "http://127.0.0.1:" + std::to_string(port) + "/";
    fprintf(stdout, "READY %s\n", url.c_str());
    fflush(stdout);

    std::thread opener([&] {
        while (!svr.is_running()) std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (!noBrowser) openBrowser(url);
    });
    // Exit when the page stops sending heartbeats (tab closed) or the user clicks Quit.
    std::thread watchdog([&] {
        while (svr.is_running() || !api.quitRequested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            const long long limit = api.everPinged() ? 20000 : 120000;   // generous before the first load
            if (api.quitRequested() || api.msSinceLastPing() > limit) { svr.stop(); return; }
        }
    });
    svr.listen_after_bind();
    opener.join();
    watchdog.join();
    return 0;
}

}  // namespace

// "--no-browser" and "--port N" are for automated tests; a double-click uses neither.
static void parseArgs(const std::string& cmd, bool& nb, int& port) {
    nb = cmd.find("--no-browser") != std::string::npos;
    port = 0;
    auto p = cmd.find("--port");
    if (p != std::string::npos) port = std::atoi(cmd.c_str() + p + 6);
}

#if defined(_WIN32) && !defined(WB_CONSOLE)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR cmd, int) {
    bool nb; int port;
    parseArgs(cmd ? cmd : "", nb, port);
    return run(nb, port);
}
#else
int main(int argc, char** argv) {
    std::string cmd;
    for (int i = 1; i < argc; ++i) cmd += std::string(argv[i]) + " ";
    bool nb; int port;
    parseArgs(cmd, nb, port);
    return run(nb, port);
}
#endif
