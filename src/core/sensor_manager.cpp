#include "sensor_manager.h"

#include <json/json.h>
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
#include <algorithm>

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
  std::atomic<bool> need_restart{false};
};

struct State {
  std::vector<std::unique_ptr<Slot>> slots; 
  std::unordered_map<std::string, uint8_t> id2sid;
  std::atomic<bool> running{false};
  std::thread th;
  std::atomic<uint32_t> seq{0};
};

State& S() {
  static State s;
  return s;
}

} // namespace

//------------------------------------------------------------------------------

SensorManager::SensorManager(AppConfig& app_config) : app_config_(app_config) {
}

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
  
  // Update app_config_ immediately
  if (id < static_cast<int>(app_config_.sensors.size())) {
    app_config_.sensors[id].pose.tx = tx;
    app_config_.sensors[id].pose.ty = ty;
    app_config_.sensors[id].pose.theta_deg = theta_deg;
  }
}

void SensorManager::setSensorMask(int id, const SensorMaskLocal& m) {
  auto& st = S();
  if (id < 0 || id >= static_cast<int>(st.slots.size())) return;
  auto& sl = *st.slots[id];
  sl.cfg.mask = m;
  
  // Update app_config_ immediately
  if (id < static_cast<int>(app_config_.sensors.size())) {
    app_config_.sensors[id].mask = m;
  }
  
  // 動的にISensorへ渡す必要があれば、ISensor拡張で対応
}

bool SensorManager::restartSensor(int id){
  auto& st = S();
  if (id < 0 || id >= static_cast<int>(st.slots.size())) return false;
  auto& sl = *st.slots[static_cast<size_t>(id)];
  if (!sl.dev) return false;

  if (sl.started) {
    sl.dev->stop();
    sl.started = false;
  }
  const bool ok = sl.dev->start(sl.cfg);
  sl.started = ok;
  sl.need_restart.store(false);
  std::cout << "[SensorManager] restart slot=" << id << " -> " << (ok?"OK":"NG") << std::endl;
  return ok;
}

bool SensorManager::applyPatch(int id, const Json::Value& patch, Json::Value& applied, std::string& err){
  auto& st = S();
  if (id < 0 || id >= static_cast<int>(st.slots.size())) { err="invalid id"; return false; }
  auto& sl = *st.slots[static_cast<size_t>(id)];
  applied = Json::Value(Json::objectValue);
  bool need_restart = false;

  // enabled / on
  if (patch.isMember("enabled") || patch.isMember("on")) {
    const bool en = patch.isMember("enabled") ? patch["enabled"].asBool() : patch["on"].asBool();
    if (!setEnabled(id, en)) { err = "failed to (en|dis)able"; return false; }
    applied["enabled"] = en;
  }

  // pose（flat or nested）
  bool poseChanged=false;
  if (patch.isMember("tx"))        { sl.cfg.pose.tx        = patch["tx"].asFloat();        poseChanged=true; }
  if (patch.isMember("ty"))        { sl.cfg.pose.ty        = patch["ty"].asFloat();        poseChanged=true; }
  if (patch.isMember("theta_deg")) { sl.cfg.pose.theta_deg = patch["theta_deg"].asFloat(); poseChanged=true; }
  if (patch.isMember("pose") && patch["pose"].isObject()){
    const auto& p = patch["pose"];
    if (p.isMember("tx"))        { sl.cfg.pose.tx        = p["tx"].asFloat();        poseChanged=true; }
    if (p.isMember("ty"))        { sl.cfg.pose.ty        = p["ty"].asFloat();        poseChanged=true; }
    if (p.isMember("theta_deg")) { sl.cfg.pose.theta_deg = p["theta_deg"].asFloat(); poseChanged=true; }
  }
  if (poseChanged){
    setPose(id, sl.cfg.pose.tx, sl.cfg.pose.ty, sl.cfg.pose.theta_deg);
    Json::Value p(Json::objectValue);
    p["tx"]=sl.cfg.pose.tx; p["ty"]=sl.cfg.pose.ty; p["theta_deg"]=sl.cfg.pose.theta_deg;
    applied["pose"]=p;
  }

  // mask
  if (patch.isMember("mask")){
    const auto& m = patch["mask"];
    if (m.isMember("angle")){
      if (m["angle"].isMember("min_deg")) sl.cfg.mask.angle.min_deg = m["angle"]["min_deg"].asFloat();
      if (m["angle"].isMember("max_deg")) sl.cfg.mask.angle.max_deg = m["angle"]["max_deg"].asFloat();
    }
    if (m.isMember("range")){
      if (m["range"].isMember("near_m")) sl.cfg.mask.range.near_m = std::max(0.0f, m["range"]["near_m"].asFloat());
      if (m["range"].isMember("far_m"))  sl.cfg.mask.range.far_m  = std::max(0.0f, m["range"]["far_m"].asFloat());
    }
    if (sl.cfg.mask.range.near_m > sl.cfg.mask.range.far_m) std::swap(sl.cfg.mask.range.near_m, sl.cfg.mask.range.far_m);
    if (sl.cfg.mask.angle.min_deg > sl.cfg.mask.angle.max_deg) std::swap(sl.cfg.mask.angle.min_deg, sl.cfg.mask.angle.max_deg);
    setSensorMask(id, sl.cfg.mask);
    applied["mask"] = Json::Value(Json::objectValue);
  }

  // endpoint
  if (patch.isMember("endpoint")){
    const auto& ep = patch["endpoint"];
    if (ep.isObject()) {
      if (ep.isMember("host")) sl.cfg.host = ep["host"].asString();
      if (ep.isMember("port")) sl.cfg.port = ep["port"].asInt();
    } else if (ep.isString()){
      const auto str = ep.asString();
      auto pos = str.find(':');
      if (pos == std::string::npos) { sl.cfg.host = str; }
      else { sl.cfg.host = str.substr(0,pos); try{ sl.cfg.port = std::stoi(str.substr(pos+1)); }catch(...){} }
    }
    Json::Value out(Json::objectValue); out["host"]=sl.cfg.host; out["port"]=sl.cfg.port;
    applied["endpoint"] = out;
    
    // Update app_config_ immediately
    if (id < static_cast<int>(app_config_.sensors.size())) {
      app_config_.sensors[id].host = sl.cfg.host;
      app_config_.sensors[id].port = sl.cfg.port;
    }
    
    need_restart = true;
  }

  // mode
  if (patch.isMember("mode")){
    const std::string m = patch["mode"].asString();
    sl.cfg.mode = m;
    applied["mode"] = sl.cfg.mode;
    
    // Update app_config_ immediately
    if (id < static_cast<int>(app_config_.sensors.size())) {
      app_config_.sensors[id].mode = m;
    }
    
    if (!sl.dev || !sl.dev->applyMode(m)) need_restart = true;
  }

  // skip_step
  if (patch.isMember("skip_step")){
    const int v = patch["skip_step"].asInt();
    if (v < 1){ err = "skip_step must be >= 1"; return false; }
    sl.cfg.skip_step = v;
    applied["skip_step"]=v;
    
    // Update app_config_ immediately
    if (id < static_cast<int>(app_config_.sensors.size())) {
      app_config_.sensors[id].skip_step = v;
    }
    
    if (!sl.dev || !sl.dev->applySkipStep(v)) need_restart = true;
  }
  if (patch.isMember("ignore_checksum_error")){
    const int v = patch["ignore_checksum_error"].asInt();
    if (v!=0 && v!=1){ err = "ignore_checksum_error must be 0 or 1"; return false; }
    sl.cfg.ignore_checksum_error = v;
    applied["ignore_checksum_error"]=v;
    
    // Update app_config_ immediately
    if (id < static_cast<int>(app_config_.sensors.size())) {
      app_config_.sensors[id].ignore_checksum_error = v;
    }
    
    need_restart = true;
  }

  if (need_restart && sl.started) {
    sl.need_restart.store(true);
    restartSensor(id);
  }
  return true;
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
    // TODO:　アプリ設定によって可変にする
    const double target_fps = 30.0;

    const auto period = std::chrono::duration_cast<clock_mono::duration>(
                          std::chrono::duration<double>(1.0 / target_fps));
    auto next_tick = clock_mono::now();

    // TODO: センサー数やセンサーの種類によって配列サイズは変えるべき
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

          auto pass_local_mask = [&](float ang, float r_m, const SensorMaskLocal& m) {
            return (m.angle.min_deg <= ang && ang <= m.angle.max_deg) &&
                   (m.range.near_m <= r_m && r_m <= m.range.far_m);
          };
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

// =============================================================================
// 追加: WS連携用最小API（enable の切替、および状態のJSON化）
// =============================================================================

bool SensorManager::setEnabled(int id, bool on) {
  auto& st = S();
  if (id < 0 || id >= static_cast<int>(st.slots.size())) return false;
  auto& sl = *st.slots[static_cast<size_t>(id)];
  if (!sl.dev) return false;

  if (on) {
    if (!sl.started) {
      if (sl.dev->start(sl.cfg)) {
        sl.started = true;
        std::cout << "[SensorManager] enabled sensor slot=" << id
                  << " (cfg.id=" << sl.cfg.id << ")\n";
      } else {
        std::cerr << "[SensorManager] FAILED to enable sensor slot=" << id
                  << " (cfg.id=" << sl.cfg.id << ")\n";
        return false;
      }
    }
  } else {
    if (sl.started) {
      sl.dev->stop();
      sl.started = false;
      std::cout << "[SensorManager] disabled sensor slot=" << id
                << " (cfg.id=" << sl.cfg.id << ")\n";
    }
  }
  return true;
}

Json::Value SensorManager::getAsJson(int id) const {
  Json::Value s(Json::objectValue);
  const auto& st = S();
  if (id < 0 || id >= static_cast<int>(st.slots.size())) return s;
  const auto& sl = *st.slots[static_cast<size_t>(id)];

  s["id"] = id;               // slot index
  s["enabled"] = sl.started;  // 実行状態
  s["cfg_id"] = sl.cfg.id;    // 設定上のセンサーID

  // endpoint + mode
  {
    Json::Value ep(Json::objectValue);
    ep["host"] = sl.cfg.host;
    ep["port"] = sl.cfg.port;
    s["endpoint"] = ep;
    s["mode"] = sl.cfg.mode;
  }
  // pose
  {
    Json::Value p(Json::objectValue);
    p["tx"]=sl.cfg.pose.tx; p["ty"]=sl.cfg.pose.ty; p["theta_deg"]=sl.cfg.pose.theta_deg;
    s["pose"]=p;
  }
  // mask
  {
    Json::Value m(Json::objectValue);
    Json::Value a(Json::objectValue); a["min_deg"]=sl.cfg.mask.angle.min_deg; a["max_deg"]=sl.cfg.mask.angle.max_deg; m["angle"]=a;
    Json::Value r(Json::objectValue); r["near_m"]=sl.cfg.mask.range.near_m; r["far_m"]=sl.cfg.mask.range.far_m; m["range"]=r;
    s["mask"]=m;
  }
  // details（詳細タブ）
  s["skip_step"]            = sl.cfg.skip_step;
  s["ignore_checksum_error"]= sl.cfg.ignore_checksum_error;
  return s;
}

Json::Value SensorManager::listAsJson() const {
  Json::Value arr(Json::arrayValue);
  const auto& st = S();
  for (int i = 0; i < static_cast<int>(st.slots.size()); ++i) {
    arr.append(getAsJson(i));
  }
  return arr;
}