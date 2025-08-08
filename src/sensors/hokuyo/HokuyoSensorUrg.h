#pragma once
#include "sensors/ISensor.h"
#include <atomic>
#include <thread>
#include <mutex>

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
    int first_{0}, last_{0};
    int skip_step_{0};

    std::atomic<bool> running_{false};
    std::thread th_;

    std::mutex cb_mu_;
    Callback cb_{};
};