// Transport layer: maps HTTP/JSON onto the C++ core. No numerical logic lives here.
#pragma once
#include <atomic>
#include <string>

#include "httplib.h"

namespace wb {

struct ServerConfig {
    std::string dataDir;      // folder holding example files
    std::string webDir;       // static front-end
};

class ApiServer {
public:
    explicit ApiServer(ServerConfig cfg);
    ~ApiServer();
    void registerRoutes(httplib::Server& svr);
    long long msSinceLastPing() const;
    bool everPinged() const { return everPinged_.load(); }
    bool quitRequested() const { return quit_.load(); }

private:
    struct Impl;
    Impl* impl_;
    std::atomic<bool> everPinged_{false};
    std::atomic<bool> quit_{false};
    std::atomic<long long> lastPingMs_{0};
};

}  // namespace wb
