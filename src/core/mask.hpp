#pragma once
#include "config/config.hpp"
#include <cmath>

inline bool pass_local_mask(float angle, float r, const SensorMaskLocal& m){
  if(angle < m.angle_min || angle > m.angle_max) return false;
  if(r < m.range_near || r > m.range_far) return false;
  return true;
}
