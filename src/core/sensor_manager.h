#pragma once

#include <functional>
#include <vector>
#include <cstdint>
#include "config/config.h"
#include <json/json.h>

struct ScanFrame {
  uint64_t t_ns; uint32_t seq;
  std::vector<float> xy;           // [x0,y0,x1,y1,...]
  std::vector<uint8_t> sid;        // センサーID
};

class SensorManager {
public:
  using FrameCallback = std::function<void(const ScanFrame&)>;
  
  // Constructor to accept AppConfig reference
  SensorManager(AppConfig& app_config);
  
  void configure(const std::vector<SensorConfig>& cfgs);
  void start(FrameCallback cb);
  void setSensorPower(int id, bool on);
  void setPose(int id, float tx, float ty, float theta_deg);
  void setSensorMask(int id, const SensorMaskLocal& m);

  // id は slots のインデックス（0..）として扱う（MVP）
  bool setEnabled(int id, bool on);
  Json::Value getAsJson(int id) const;
  Json::Value listAsJson() const;

  bool applyPatch(int id, const Json::Value& patch, Json::Value& applied, std::string& err);
  bool restartSensor(int id);
  
  // Reload configuration from AppConfig (for Load/Import operations)
  void reloadFromAppConfig();

private:
  AppConfig& app_config_;  // Reference to main config for immediate updates
};
