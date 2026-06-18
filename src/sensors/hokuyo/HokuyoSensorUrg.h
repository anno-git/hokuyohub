#pragma once
#include "sensors/ISensor.h"
#include <atomic>
#include <thread>
#include <mutex>

// URG library の urg_tcpclient.h は Windows で <windows.h> しか include
// していないが、必要な sockaddr_in は <winsock2.h> に居る。winsock2.h は
// 必ず windows.h より前に include しないと winsock 1 系と競合する。
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
#endif

extern "C" {
  #include "urg_sensor.h"
  #include "urg_utils.h"
}

class HokuyoSensorUrg final : public ISensor {
public:
    explicit HokuyoSensorUrg();
    ~HokuyoSensorUrg() override;

    bool start(const SensorConfig& cfg) override;
    void stop() override;
    void subscribe(Callback cb) override;

private:
    bool openAndConfigure();
    void closeDevice();
    void rxLoop(urg_measurement_type_t mtype);
    static urg_measurement_type_t toUrgMode(const std::string& mode);

    std::string id_;
    SensorConfig cfg_{};
    urg_t urg_{};
    std::atomic<bool> opened_{false};

    std::atomic<bool> running_{false};
    std::thread th_;

    std::mutex cb_mu_;
    Callback cb_{};
};