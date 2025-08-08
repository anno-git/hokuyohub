#pragma once

#include "config/config.h"
#include <cmath>

inline bool pass_local_mask(float angle, float r, const SensorMaskLocal& m){
  if(angle < m.angle.min_deg || angle > m.angle.max_deg) return false;
  if(r < m.range.near_m || r > m.range.far_m) return false;
  return true;
}
