#include "sensor_manager.h"

#include "sensors/ISensor.h"
#include "sensors/SensorFactory.h"
#include "mask.h"
#include "transform.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

using clock_mono = std::chrono::steady_clock;
using clock_sys  = std::chrono::system_clock;
using std::chrono::nanoseconds;

namespace {

// step -> angle [rad]; 0 rad is front_step, positive CCW
inline float step_to_angle_rad(int step, int front_step, double ang_res_deg) {
  const double ddeg = (static_cast<double>(step) - static_cast<double>(front_step)) * ang_res_deg;
  return static_cast<float>(ddeg * (M_PI / 180.0));
}

struct Slot {
  SensorConfig cfg;                    // pose/mask/connection 等を保持（動的更新もここ）
  std::unique_ptr<ISensor> dev;        // 実デバイス（抽象）
  RawScan latest;                      // 最新Raw（push購読で差し替え）
  std::mutex mu;                       // latest保護
  uint8_t sid{0};                      // 出力時のセンサーID（0..255）
  bool started{false};
};

struct State {
  // ★ vector<unique_ptr<Slot>> に変更（mutex含む型は再配置できない）
  std::vector<std::unique_ptr<Slot>> slots; 
  std::unordered_map<std::string, uint8_t> id2sid;
  std::atomic<bool> running{false};
  std::thread th;                      // 集約スレッド
  std::atomic<uint32_t> seq{0};
};

State& S() {
  static State s;
  return s;
}

} // namespace

//------------------------------------------------------------------------------

void SensorManager::configure(const std::vector<SensorConfig>& cfgs) {
  auto& st = S();
  if (st.running.load()) {
    std::cerr << "[SensorManager] configure() ignored while running.\n";
    return;
  }
  st.slots.clear();
  st.id2sid.clear();
  st.seq.store(0);

  uint8_t next_sid = 0;
  for (const auto& c : cfgs) {
    if (!c.enabled) continue;

    auto p = std::make_unique<Slot>();
    p->cfg = c;
    p->sid = next_sid++;
    p->dev = create_sensor(c);
    if (!p->dev) {
      std::cerr << "[SensorManager] no driver for type: " << c.type << " (id=" << c.id << ")\n";
      continue;
    }
    // センサからのpushを受け、「最新だけ」保持
    p->dev->subscribe([raw = p.get()](const RawScan& rs){
      std::cout << "[SensorManager] received scan from " << raw->cfg.id << std::endl;
	  // print first data
	  std::cout << "  first range: " << rs.ranges_mm.front() << " mm" << std::endl;
      std::lock_guard<std::mutex> lk(raw->mu);
      raw->latest = rs;
    });
    st.id2sid[c.id] = p->sid;
    st.slots.emplace_back(std::move(p));
  }

  std::cout << "[SensorManager] configured sensors=" << st.slots.size() << std::endl;
}

void SensorManager::setSensorPower(int /*id*/, bool /*on*/) {
  // 現状、Hokuyoは電源制御API無し。必要なら将来 ISensor に拡張。
}

void SensorManager::setPose(int id, float tx, float ty, float theta_deg) {
  auto& st = S();
  if (id < 0 || id >= static_cast<int>(st.slots.size())) return;
  auto& sl = *st.slots[id];
  sl.cfg.pose.tx   = tx;
  sl.cfg.pose.ty   = ty;
  sl.cfg.pose.theta_deg = theta_deg; // pose.theta_deg は [deg] 想定
}

void SensorManager::setSensorMask(int id, const SensorMaskLocal& m) {
  auto& st = S();
  if (id < 0 || id >= static_cast<int>(st.slots.size())) return;
  auto& sl = *st.slots[id];
  sl.cfg.mask = m;
  // 動的にISensorへ渡す必要があれば、ISensor拡張で対応
}

void SensorManager::start(FrameCallback cb) {
  auto& st = S();
  if (st.running.exchange(true)) {
    std::cerr << "[SensorManager] already running.\n";
    return;
  }

  // 各センサーを起動
  for (auto& up : st.slots) {
    auto& sl = *up;
    if (!sl.dev) continue;
    if (sl.dev->start(sl.cfg)) {
      sl.started = true;
      std::cout << "[SensorManager] started sensor id=" << sl.cfg.id
                << " (sid=" << int(sl.sid) << ")\n";
    } else {
      std::cerr << "[SensorManager] FAILED to start sensor id=" << sl.cfg.id << "\n";
    }
  }

  // 集約スレッド：直近Rawを統合してScanFrameに
  st.th = std::thread([cb]{
    auto& st2 = S();
    const double target_fps = 30.0;

    // ★ clock_mono::duration へキャストしておく
    const auto period = std::chrono::duration_cast<clock_mono::duration>(
                          std::chrono::duration<double>(1.0 / target_fps));
    auto next_tick = clock_mono::now();

    std::vector<float> xy;  xy.reserve(16384);
    std::vector<uint8_t> sid; sid.reserve(8192);

    while (st2.running.load()) {
      xy.clear();
      sid.clear();

      for (auto& up : st2.slots) {
        auto& sl = *up;
        if (!sl.started) continue;

        RawScan rs;
        {
          std::lock_guard<std::mutex> lk(sl.mu);
          rs = sl.latest; // 無い場合は空
        }
        if (rs.ranges_mm.empty()) continue;

        const auto& pose = sl.cfg.pose;
        const auto& m    = sl.cfg.mask;

        int step = rs.start_step;
        const int N = static_cast<int>(rs.ranges_mm.size());
        for (int i = 0; i < N; ++i, ++step) {
          const uint16_t d_mm = rs.ranges_mm[i];
          if (d_mm == 0) continue; // 欠測
          const float r_m = static_cast<float>(d_mm) * 0.001f;
          const float ang = step_to_angle_rad(step, rs.front_step, rs.ang_res_deg);

          if (!pass_local_mask(ang, r_m, m)) continue;

          float x = r_m * std::cos(ang);
          float y = r_m * std::sin(ang);
          apply_pose(x, y, pose.tx, pose.ty, pose.theta_deg * (M_PI / 180.0f));

          xy.push_back(x);
          xy.push_back(y);
          sid.push_back(sl.sid);
        }
      }

      ScanFrame f;
      f.seq  = st2.seq.fetch_add(1);
      f.t_ns = std::chrono::duration_cast<nanoseconds>(
                 clock_sys::now().time_since_epoch()).count();
      f.xy   = xy;
      f.sid  = sid;
      cb(f);

      next_tick += period;                       // ★ 同一duration型で加算
      std::this_thread::sleep_until(next_tick);
    }

    // 停止時
    for (auto& up : st2.slots) {
      auto& sl = *up;
      if (sl.dev) sl.dev->stop();
      sl.started = false;
    }
  });

  // NOTE: stop/join は未実装（MVP常駐）。必要になったらメソッド追加。
}