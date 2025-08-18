#pragma once
#include <vector>

namespace core {

struct Point2D {
  double x{0.0};
  double y{0.0};
  
  Point2D() = default;
  Point2D(double x_, double y_) : x(x_), y(y_) {}
};

struct Polygon {
  std::vector<Point2D> points;
  
  bool contains(const Point2D& p) const;
  bool empty() const { return points.size() < 3; }
};

struct WorldMask {
  std::vector<Polygon> include;
  std::vector<Polygon> exclude;

  bool empty() const { return include.empty() && exclude.empty(); }
  bool allows(const Point2D& p) const;
};

} // namespace core
