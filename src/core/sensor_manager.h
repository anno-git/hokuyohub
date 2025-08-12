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
  void configure(const std::vector<SensorConfig>& cfgs);
  void start(FrameCallback cb);
  void setSensorPower(int id, bool on);
  void setPose(int id, float tx, float ty, float theta_deg);
  void setSensorMask(int id, const SensorMaskLocal& m);

  // id は slots のインデックス（0..）として扱う（MVP）
  bool setEnabled(int id, bool on);
  Json::Value getAsJson(int id) const;
  Json::Value listAsJson() const;
};
