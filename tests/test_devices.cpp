#include <cstdio>

#include "devices.hpp"

static int g_fail = 0;
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++g_fail;                                                   \
        }                                                               \
    } while (0)

int main() {
    using namespace wb;
    MockDeviceManager m;

    auto devs = m.discover();
    CHECK(devs.size() == 3);
    bool mcu = false, fpga = false, jtag = false, allSim = true;
    for (auto& d : devs) {
        mcu |= d.type == DeviceType::Microcontroller;
        fpga |= d.type == DeviceType::Fpga;
        jtag |= d.type == DeviceType::JtagAdapter;
        allSim &= d.simulated && d.state == DeviceState::Disconnected;
    }
    CHECK(mcu && fpga && jtag && allSim);

    std::vector<std::string> caps;
    CHECK(m.capabilities("jtag0", caps).ok && !caps.empty());
    CHECK(!m.capabilities("nope", caps).ok);

    // Telemetry before connect is rejected; after connect it works for all three.
    Telemetry t;
    CHECK(m.readTelemetry("mcu0", t).error == DeviceError::NotConnected);
    for (const char* id : {"mcu0", "fpga0", "jtag0"}) {
        CHECK(m.connect(id).ok);
        CHECK(m.connect(id).error == DeviceError::AlreadyConnected);
        CHECK(m.readTelemetry(id, t).ok && t.simulated && !t.values.empty() && t.deviceId == id);
    }
    unsigned long long s1 = t.sequence;
    m.readTelemetry("jtag0", t);
    CHECK(t.sequence == s1 + 1);
    for (const char* id : {"mcu0", "fpga0", "jtag0"}) CHECK(m.disconnect(id).ok);
    CHECK(m.disconnect("mcu0").error == DeviceError::NotConnected);
    CHECK(m.connect("zzz").error == DeviceError::NotFound);

    // Recoverable failure: two timeouts, then success on the 3rd attempt.
    m.injectTimeouts("fpga0", 2);
    CHECK(m.connect("fpga0").error == DeviceError::Timeout);
    auto r = m.connectWithRetry("fpga0", 3);
    CHECK(r.ok && r.attempts == 2);  // one timeout left, then success
    // Timeout on telemetry leaves the device connected and recoverable.
    m.injectTimeouts("fpga0", 1);
    CHECK(m.readTelemetry("fpga0", t).error == DeviceError::Timeout);
    CHECK(m.readTelemetry("fpga0", t).ok);
    // Gives up honestly when retries are exhausted.
    m.disconnect("mcu0");
    m.injectTimeouts("mcu0", 5);
    auto g = m.connectWithRetry("mcu0", 2);
    CHECK(!g.ok && g.error == DeviceError::Timeout && g.attempts == 2);

    if (g_fail == 0) std::printf("test_devices: all checks passed\n");
    return g_fail == 0 ? 0 : 1;
}
