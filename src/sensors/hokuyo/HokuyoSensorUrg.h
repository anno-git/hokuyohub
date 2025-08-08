#pragma once
#include "sensors/ISensor.h"
#include <atomic>
#include <thread>

extern "C" {
  #include "urg_sensor.h"
  #include "urg_utils.h"
}

class HokuyoSensorUrg final : public ISensor {
public:
    explicit HokuyoSensorUrg(std::string id);
    ~HokuyoSensorUrg() override;

    bool start(const SensorConfig& cfg) override;
    void stop() override;
    void subscribe(Callback cb) override { cb_ = std::move(cb); }

private:
    void rxLoop(urg_measurement_type_t mtype);

    std::string id_;
    SensorConfig cfg_{};
    urg_t urg_{};
    bool opened_{false};
    int first_{0}, last_{0};

    std::atomic<bool> running_{false};
    std::thread th_;
    Callback cb_{};
};