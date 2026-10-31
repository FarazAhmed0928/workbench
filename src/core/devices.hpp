// Task E: SIMULATED device adapters. Nothing here talks to hardware.
// Every record carries simulated=true so the UI can label it clearly.
#pragma once
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace wb {

enum class DeviceType { Microcontroller, Fpga, JtagAdapter };
enum class DeviceState { Disconnected, Connected };
enum class DeviceError { None, NotFound, NotConnected, AlreadyConnected, Timeout };

std::string toString(DeviceType t);
std::string toString(DeviceState s);
std::string toString(DeviceError e);

struct DeviceInfo {
    std::string id;
    DeviceType type = DeviceType::Microcontroller;
    std::string name;
    std::string role;                       // what the device is for
    std::vector<std::string> capabilities;
    DeviceState state = DeviceState::Disconnected;
    bool simulated = true;
};

struct TelemetryValue {
    std::string name;
    double value = 0;
    std::string unit;
};

struct Telemetry {
    std::string deviceId;
    unsigned long long sequence = 0;
    std::vector<TelemetryValue> values;
    bool simulated = true;
};

struct DeviceStatus {
    bool ok = true;
    DeviceError error = DeviceError::None;
    std::string message;      // actionable text for the UI
    int attempts = 1;         // how many tries it took (for the recovery demo)
};

class MockDeviceManager {
public:
    MockDeviceManager();

    std::vector<DeviceInfo> discover() const;
    DeviceStatus capabilities(const std::string& id, std::vector<std::string>& out) const;
    DeviceStatus connect(const std::string& id);
    DeviceStatus disconnect(const std::string& id);
    DeviceStatus readTelemetry(const std::string& id, Telemetry& out);

    // Failure injection: the next `count` connect/telemetry calls to this device time out.
    void injectTimeouts(const std::string& id, int count);
    // Recovery policy: retry connect up to maxAttempts (simulated back-off, no real sleeping).
    DeviceStatus connectWithRetry(const std::string& id, int maxAttempts);

private:
    mutable std::mutex m_;
    std::vector<DeviceInfo> devices_;
    std::map<std::string, int> pendingTimeouts_;
    std::map<std::string, unsigned long long> seq_;
    DeviceInfo* find(const std::string& id);
    const DeviceInfo* find(const std::string& id) const;
    bool consumeTimeout(const std::string& id);
};

}  // namespace wb
