#include "mask.h"
#include <cmath>

namespace core {

bool Polygon::contains(const Point2D& p) const {
  if (points.size() < 3) return false;
  
  bool inside = false;
  size_t n = points.size();
  
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
    const auto& pi = points[i];
    const auto& pj = points[j];
    
    if (((pi.y > p.y) != (pj.y > p.y)) &&
        (p.x < (pj.x - pi.x) * (p.y - pi.y) / (pj.y - pi.y) + pi.x)) {
      inside = !inside;
    }
  }
  
  return inside;
}

bool WorldMask::allows(const Point2D& p) const {
  // If no include polygons, point is allowed by default
  bool included = include.empty();
  
  // Check if point is in any include polygon
  if (!included) {
    for (const auto& poly : include) {
      if (poly.contains(p)) {
        included = true;
        break;
      }
    }
  }
  
  // If not included, reject
  if (!included) return false;
  
  // Check if point is in any exclude polygon
  for (const auto& poly : exclude) {
    if (poly.contains(p)) {
      return false;
    }
  }
  
  return true;
}

} // namespace core