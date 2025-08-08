#include "HokuyoSensorUrg.h"
#include <chrono>
#include <vector>
#include <algorithm>

using clock_mono = std::chrono::steady_clock;

HokuyoSensorUrg::HokuyoSensorUrg(std::string id)
: id_(std::move(id)) {
    // urg_ はゼロ初期化済み
}

HokuyoSensorUrg::~HokuyoSensorUrg() {
    stop();
}

bool HokuyoSensorUrg::start(const SensorConfig& cfg) {
    cfg_ = cfg;

    // Ethernet接続: URG_ETHERNET / IP / 10940
    if (urg_open(&urg_, URG_ETHERNET, cfg_.host.c_str(), cfg_.port) < 0) {
        return false;
    }
    opened_ = true;
    urg_set_timeout_msec(&urg_, 2000);

    int min_step = 0, max_step = 0;
    urg_step_min_max(&urg_, &min_step, &max_step);

    // start_step / end_step 未指定ならPPに合わせる
    first_ = (cfg_.start_step == 0 && cfg_.end_step == 0)
           ? min_step : std::max(cfg_.start_step, min_step);
    last_  = (cfg_.start_step == 0 && cfg_.end_step == 0)
           ? max_step : std::min(cfg_.end_step,   max_step);

    // cluster は urg 的には "skip"（間引き）扱い（0=間引きなし）
    const int skip = std::max(0, cfg_.cluster - 1);
    if (urg_set_scanning_parameter(&urg_, first_, last_, skip) < 0) {
        urg_close(&urg_); opened_ = false; return false;
    }

    // 強度あり/なし
    const auto mtype = (cfg_.mode == "ME") ? URG_DISTANCE_INTENSITY : URG_DISTANCE;

    // 連続スキャン開始（URG_SCAN_INFINITY = 無限）
	bool ignore_checksum_error = true;
    if (urg_start_measurement(&urg_, mtype, URG_SCAN_INFINITY, cfg_.interval, ignore_checksum_error) < 0) {
        urg_close(&urg_); opened_ = false; return false;
    }

    running_ = true;
    th_ = std::thread([this, mtype]{ rxLoop(mtype); });
    return true;
}

void HokuyoSensorUrg::stop() {
    running_ = false;
    if (th_.joinable()) th_.join();

    if (opened_) {
        urg_stop_measurement(&urg_);
        urg_close(&urg_);
        opened_ = false;
    }
}

void HokuyoSensorUrg::rxLoop(urg_measurement_type_t mtype) {
    const int nmax = urg_max_data_size(&urg_);
    std::vector<long> dist(nmax);
    std::vector<unsigned short> inten(nmax);

    while (running_) {
        long ts_sensor = 0;
        int n = 0;

        if (mtype == URG_DISTANCE_INTENSITY) {
            n = urg_get_distance_intensity(&urg_, dist.data(), inten.data(), &ts_sensor);
        } else {
            n = urg_get_distance(&urg_, dist.data(), &ts_sensor);
        }

        if (n <= 0) {
            // タイムアウトや一時的な失敗。軽くスリープしてリトライ。
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        RawScan out;
        out.monotonic_ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            clock_mono::now().time_since_epoch()).count();

        out.ranges_mm.assign(dist.begin(), dist.begin() + n);
        if (mtype == URG_DISTANCE_INTENSITY) {
            out.intensities.assign(inten.begin(), inten.begin() + n);
        }

        out.start_step = first_;
        out.end_step   = first_ + (n - 1) * (1 + std::max(0, cfg_.cluster - 1)); // 粗い近似（skip反映）
        // 角度分解能（度）
        const double deg0 = urg_step2deg(&urg_, first_);
        const double deg1 = urg_step2deg(&urg_, first_ + 1 + std::max(0, cfg_.cluster - 1));
        out.ang_res_deg = deg1 - deg0;
        out.front_step  = urg_deg2step(&urg_, 0.0);
        out.sensor_id   = cfg_.id;

        if (cb_) cb_(out);
    }
}