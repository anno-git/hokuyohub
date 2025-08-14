#pragma once

#include "config/config.h"
#include <cmath>

// Pass local mask filter
// @param angle: angle in radians
// @param r: range in meters
// @param m: mask configuration (angles in degrees, ranges in meters)
// @return true if point passes the mask filter
inline bool pass_local_mask(float angle, float r, const SensorMaskLocal& m){
  // Convert angle from radians to degrees for comparison with mask thresholds
  const float angle_deg = angle * 180.0f / M_PI;
  if(angle_deg < m.angle.min_deg || angle_deg > m.angle.max_deg) return false;
  if(r < m.range.near_m || r > m.range.far_m) return false;
  return true;
}
