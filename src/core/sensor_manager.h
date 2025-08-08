#pragma once

#include <functional>
#include <vector>
#include <cstdint>
#include "config/config.h"

struct ScanFrame {
  uint64_t t_ns; uint32_t seq;
  // SoA：xyとsensor id
  std::vector<float> xy;           // [x0,y0,x1,y1,...]
  std::vector<uint8_t> sid;        // センサーID
};

class SensorManager {
public:
  using FrameCallback = std::function<void(const ScanFrame&)>;
  void configure(const std::vector<SensorCfg>& cfgs);
  void start(FrameCallback cb);
  void setSensorPower(int id, bool on);
  void setPose(int id, float tx, float ty, float theta);
  void setSensorMask(int id, const SensorMaskLocal& m);
};