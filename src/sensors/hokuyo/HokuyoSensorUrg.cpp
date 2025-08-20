#include "HokuyoSensorUrg.h"
#include <chrono>
#include <vector>
#include <algorithm>
#include <cstring>
#include <iostream>

using clock_mono = std::chrono::steady_clock;

HokuyoSensorUrg::HokuyoSensorUrg()
{
    std::memset(&urg_, 0, sizeof(urg_));
}

HokuyoSensorUrg::~HokuyoSensorUrg() {
    stop();
}

bool HokuyoSensorUrg::start(const SensorConfig& cfg) {
    if (running_) return true;
    cfg_ = cfg;

    if (!openAndConfigure()) {
        return false;
    }

    running_ = true;
    const auto mtype = toUrgMode(cfg_.mode); // 5) 強度あり/なしの切替
    th_ = std::thread([this, mtype] { rxLoop(mtype); });
    return true;
}

void HokuyoSensorUrg::stop() {
    running_ = false;
    if (th_.joinable()) th_.join();
    closeDevice();
}

void HokuyoSensorUrg::subscribe(Callback cb) {
    std::lock_guard<std::mutex> lk(cb_mu_);
    cb_ = std::move(cb);
}

bool HokuyoSensorUrg::openAndConfigure() {
    closeDevice();

    // open
	std::cout << "[HokuyoSensorUrg] opening " << cfg_.host << ":" << cfg_.port << std::endl;
    if (urg_open(&urg_, URG_ETHERNET, cfg_.host.c_str(), static_cast<long>(cfg_.port)) < 0) {
        std::cerr << "[HokuyoSensorUrg] openethernet failed: " << urg_error(&urg_) << std::endl;
        return false;
    }
    opened_ = true;

    // 1) 角度マスクをstepに変換して設定（min/maxの順序も正規化）
    int first = urg_deg2step(&urg_, cfg_.mask.angle.min_deg);
    int last  = urg_deg2step(&urg_, cfg_.mask.angle.max_deg);
    if (first > last) std::swap(first, last);
    first_ = first;
    last_  = last;

    // ダウンサンプル（cluster=1ならskip=0）
    skip_step_ = std::max(1, cfg_.skip_step);

    if (urg_set_scanning_parameter(&urg_, first_, last_, skip_step_) < 0) {
        std::cerr << "[HokuyoSensorUrg] set_scanning_parameter failed: " << urg_error(&urg_) << std::endl;
        closeDevice();
        return false;
    }
    // ※ 1) 距離マスク(near/far)はデバイス設定が無いのでここでは未適用（上流で実施）

    return true;
}

void HokuyoSensorUrg::closeDevice() {
    if (opened_) {
        // best-effort
        urg_stop_measurement(&urg_);
        urg_close(&urg_);
        opened_ = false;
    }
}

urg_measurement_type_t HokuyoSensorUrg::toUrgMode(const std::string& mode) {
    // "ME" = Distance + Intensity, "MD" = Distance only
    if (mode == "ME") return URG_DISTANCE_INTENSITY;
    return URG_DISTANCE;
}

void HokuyoSensorUrg::rxLoop(urg_measurement_type_t mtype) {
    // 3) 起動時のエラーハンドリング＋ワンショット再試行
    if (urg_start_measurement(&urg_, mtype, URG_SCAN_INFINITY, cfg_.interval, cfg_.ignore_checksum_error) < 0) {
        std::cerr << "[HokuyoSensorUrg] start_measurement failed: " << urg_error(&urg_) << std::endl;
        closeDevice();
        if (!openAndConfigure() || urg_start_measurement(&urg_, mtype, URG_SCAN_INFINITY, cfg_.interval, cfg_.ignore_checksum_error) < 0) {
            std::cerr << "[HokuyoSensorUrg] start_measurement retry failed: " << urg_error(&urg_) << std::endl;
            running_ = false;
            return;
        }
    }

    const int nmax = urg_max_data_size(&urg_);
    std::vector<long> dist(nmax);
    std::vector<unsigned short> inten(nmax);

    int fail_count = 0;
    while (running_) {
        const auto t0 = clock_mono::now();

        int n = 0;
        long ts = 0;
        if (mtype == URG_DISTANCE_INTENSITY) {
            n = urg_get_distance_intensity(&urg_, dist.data(), inten.data(), &ts);
        } else {
            n = urg_get_distance(&urg_, dist.data(), &ts);
        }

        if (n <= 0) {
            // 3) 受信失敗のリトライと再接続
            ++fail_count;
            if (fail_count >= 3) {
                std::cerr << "[HokuyoSensorUrg] read failed " << fail_count
                          << " times: " << urg_error(&urg_) << " - reconnecting\n";
                urg_stop_measurement(&urg_);
                closeDevice();
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                if (!openAndConfigure() ||
                    urg_start_measurement(&urg_, mtype, URG_SCAN_INFINITY, cfg_.interval, cfg_.ignore_checksum_error) < 0) {
                    std::cerr << "[HokuyoSensorUrg] reconnect failed: " << urg_error(&urg_) << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }
                fail_count = 0;
                continue;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        fail_count = 0;

        RawScan out;
        // 2) Poseは上流で適用（ここではcfg_保持のみ）。RawScanメタを充足。
        out.monotonic_ts_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(t0.time_since_epoch()).count();

        out.ranges_mm.resize(n);
        for (int i = 0; i < n; ++i) {
            long d = dist[i];
            if (d < 0) d = 0;
            if (d > 65535) d = 65535;
            out.ranges_mm[i] = static_cast<uint16_t>(d);
        }

        if (mtype == URG_DISTANCE_INTENSITY) {
            out.intensities.assign(inten.begin(), inten.begin() + n);
        } else {
            out.intensities.clear();
        }

        out.start_angle = urg_index2deg(&urg_, 0);
        out.angle_res = urg_index2deg(&urg_, 1) - out.start_angle;
        out.sensor_id = cfg_.id;

        // 4) コールバックのスレッド安全な呼び出し
        Callback cb_copy;
        {
            std::lock_guard<std::mutex> lk(cb_mu_);
            cb_copy = cb_;
        }
        if (cb_copy) cb_copy(out);
    }

    // 終了時クリーンアップ
    urg_stop_measurement(&urg_);
}