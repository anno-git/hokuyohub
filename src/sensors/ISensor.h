#pragma once
#include <vector>
#include <functional>
#include <cstdint>
#include <string>

struct SensorPose {
    double x{0}, y{0}, yaw_deg{0};
};

struct AngleMaskDeg { double min_deg{-180}, max_deg{180}; };
struct RangeMaskMm  { uint16_t min_mm{0}, max_mm{65000}; };

struct SensorConfig {
    std::string id;           // "u1" など
    std::string type;         // "hokuyo_eth"
    std::string host{"192.168.0.10"};
    uint16_t port{10940};
    std::string mode{"ME"};   // "MD" or "ME"
    int start_step{0};        // 0/1080は機種依存。未指定ならPPから補正
    int end_step{0};
    int cluster{1};           // ※urgではskip相当（ダウンサンプリング）
    int interval{0};          // 0=全スキャンごと
    int count{0};             // 0=無限（urgは別指定: URG_SCAN_INFINITY を使用）
    AngleMaskDeg angle_mask{};
    RangeMaskMm  range_mask{};
    SensorPose   pose{};
};

struct RawScan {
    uint64_t monotonic_ts_ns{0};      // 受信時刻（モノトニック）
    std::vector<uint16_t> ranges_mm;  // ステップ順
    std::vector<uint16_t> intensities;// 空なら未取得
    int start_step{0};
    int end_step{0};
    double ang_res_deg{0.0};
    int front_step{0};
    std::string sensor_id;
};

class ISensor {
public:
    using Callback = std::function<void(const RawScan&)>;
    virtual ~ISensor() = default;
    virtual bool start(const SensorConfig& cfg) = 0;
    virtual void stop() = 0;
    virtual void subscribe(Callback cb) = 0;
};