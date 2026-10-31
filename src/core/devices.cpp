#include "devices.hpp"

#include <cmath>

namespace wb {

std::string toString(DeviceType t) {
    switch (t) {
        case DeviceType::Microcontroller: return "microcontroller";
        case DeviceType::Fpga: return "fpga";
        case DeviceType::JtagAdapter: return "jtag_adapter";
    }
    return "unknown";
}
std::string toString(DeviceState s) { return s == DeviceState::Connected ? "connected" : "disconnected"; }
std::string toString(DeviceError e) {
    switch (e) {
        case DeviceError::None: return "none";
        case DeviceError::NotFound: return "not_found";
        case DeviceError::NotConnected: return "not_connected";
        case DeviceError::AlreadyConnected: return "already_connected";
        case DeviceError::Timeout: return "timeout";
    }
    return "unknown";
}

MockDeviceManager::MockDeviceManager() {
    DeviceInfo mcu;
    mcu.id = "mcu0";
    mcu.type = DeviceType::Microcontroller;
    mcu.name = "Simulated MCU board (3.3 V, USB-serial)";
    mcu.role = "Sensor and control node; reports board temperature and supply voltage";
    mcu.capabilities = {"uart_telemetry", "gpio_3v3", "adc_12bit"};

    DeviceInfo fpga;
    fpga.id = "fpga0";
    fpga.type = DeviceType::Fpga;
    fpga.name = "Simulated FPGA board (3.3 V I/O, 1.2 V core)";
    fpga.role = "Programmable logic target; reports configuration state and clock";
    fpga.capabilities = {"uart_telemetry", "jtag_target", "gpio_3v3"};

    DeviceInfo jtag;
    jtag.id = "jtag0";
    jtag.type = DeviceType::JtagAdapter;
    jtag.name = "Simulated JTAG debug adapter (VTref-sensing)";
    jtag.role = "Debug interface only: drives TCK/TMS/TDI and reads TDO on a target. Not a compute board";
    jtag.capabilities = {"jtag_chain_scan", "vtref_sense", "tck_config"};

    devices_ = {mcu, fpga, jtag};
}

DeviceInfo* MockDeviceManager::find(const std::string& id) {
    for (auto& d : devices_) if (d.id == id) return &d;
    return nullptr;
}
const DeviceInfo* MockDeviceManager::find(const std::string& id) const {
    for (auto& d : devices_) if (d.id == id) return &d;
    return nullptr;
}

bool MockDeviceManager::consumeTimeout(const std::string& id) {
    auto it = pendingTimeouts_.find(id);
    if (it == pendingTimeouts_.end() || it->second <= 0) return false;
    --it->second;
    return true;
}

std::vector<DeviceInfo> MockDeviceManager::discover() const {
    std::lock_guard<std::mutex> lk(m_);
    return devices_;
}

DeviceStatus MockDeviceManager::capabilities(const std::string& id, std::vector<std::string>& out) const {
    std::lock_guard<std::mutex> lk(m_);
    const DeviceInfo* d = find(id);
    if (!d) return {false, DeviceError::NotFound, "Unknown device '" + id + "'. Run discovery first.", 1};
    out = d->capabilities;
    return {};
}

DeviceStatus MockDeviceManager::connect(const std::string& id) {
    std::lock_guard<std::mutex> lk(m_);
    DeviceInfo* d = find(id);
    if (!d) return {false, DeviceError::NotFound, "Unknown device '" + id + "'. Run discovery first.", 1};
    if (d->state == DeviceState::Connected) return {false, DeviceError::AlreadyConnected, d->name + " is already connected.", 1};
    if (consumeTimeout(id))
        return {false, DeviceError::Timeout, "Timed out waiting for " + d->name + " (simulated). Retry, or check the cable and power.", 1};
    d->state = DeviceState::Connected;
    return {};
}

DeviceStatus MockDeviceManager::disconnect(const std::string& id) {
    std::lock_guard<std::mutex> lk(m_);
    DeviceInfo* d = find(id);
    if (!d) return {false, DeviceError::NotFound, "Unknown device '" + id + "'.", 1};
    if (d->state != DeviceState::Connected) return {false, DeviceError::NotConnected, d->name + " is not connected.", 1};
    d->state = DeviceState::Disconnected;
    return {};
}

DeviceStatus MockDeviceManager::readTelemetry(const std::string& id, Telemetry& out) {
    std::lock_guard<std::mutex> lk(m_);
    const DeviceInfo* d = find(id);
    if (!d) return {false, DeviceError::NotFound, "Unknown device '" + id + "'.", 1};
    if (d->state != DeviceState::Connected) return {false, DeviceError::NotConnected, "Connect " + d->name + " before reading telemetry.", 1};
    if (consumeTimeout(id))
        return {false, DeviceError::Timeout, "Telemetry read timed out (simulated). The device stays connected; try again.", 1};
    const unsigned long long n = ++seq_[id];
    out = Telemetry{};
    out.deviceId = id;
    out.sequence = n;
    const double ph = static_cast<double>(n);
    switch (d->type) {
        case DeviceType::Microcontroller:
            out.values = {{"board_temperature", 31.0 + 0.5 * std::sin(ph), "degC"}, {"supply_voltage", 3.30 + 0.01 * std::cos(ph), "V"}};
            break;
        case DeviceType::Fpga:
            out.values = {{"configured", 1.0, "bool"}, {"fabric_clock", 100.0, "MHz"}, {"core_voltage", 1.20 + 0.005 * std::sin(ph), "V"}};
            break;
        case DeviceType::JtagAdapter:
            out.values = {{"target_vref", 3.30, "V"}, {"tck", 1000.0, "kHz"}, {"devices_in_chain", 1.0, "count"}};
            break;
    }
    return {};
}

void MockDeviceManager::injectTimeouts(const std::string& id, int count) {
    std::lock_guard<std::mutex> lk(m_);
    pendingTimeouts_[id] = count;
}

DeviceStatus MockDeviceManager::connectWithRetry(const std::string& id, int maxAttempts) {
    DeviceStatus s;
    for (int a = 1; a <= maxAttempts; ++a) {
        s = connect(id);
        s.attempts = a;
        if (s.ok || s.error != DeviceError::Timeout) return s;
        // simulated exponential back-off: 100 ms, 200 ms, ... (not actually slept)
    }
    s.message += " Gave up after " + std::to_string(maxAttempts) + " attempts.";
    return s;
}

}  // namespace wb
