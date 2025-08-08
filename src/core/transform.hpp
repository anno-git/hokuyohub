#pragma once
#include <cmath>
inline void apply_pose(float& x, float& y, float tx, float ty, float th){
  const float c=std::cos(th), s=std::sin(th);
  float nx = c*x - s*y + tx;
  float ny = s*x + c*y + ty;
  x = nx; y = ny;
}