#pragma once
#include <vector>
#include <functional>
#include <cstdint>
#include <string>
#include "config/config.h"

struct RawScan {
    uint64_t monotonic_ts_ns{0};      // 受信時刻（モノトニック）
    std::vector<uint16_t> ranges_mm;  // ステップ順
    std::vector<uint16_t> intensities;// 空なら未取得
    double start_angle{0.0};
    double angle_res{0.25};
    std::string sensor_id{""};
};

class ISensor {
public:
    using Callback = std::function<void(const RawScan&)>;
    virtual ~ISensor() = default;
    virtual bool start(const SensorConfig& cfg) = 0;
    virtual void stop() = 0;
    virtual void subscribe(Callback cb) = 0;
    virtual bool applySkipStep(int) { return false; }
    virtual bool applyMode(const std::string&) { return false; }
};