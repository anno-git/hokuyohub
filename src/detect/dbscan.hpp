#pragma once
#include <vector>
#include <cstdint>
#include <span>

struct Cluster { uint32_t id; uint8_t sensor_mask; float cx,cy,minx,miny,maxx,maxy; uint16_t count; };

class DBSCAN2D {
  float eps_; int minPts_;
public:
  DBSCAN2D(float eps, int minPts): eps_(eps), minPts_(minPts){}
  void setParams(float eps, int minPts){ eps_=eps; minPts_=minPts; }
  std::vector<Cluster> run(std::span<const float> xy, std::span<const uint8_t> sid, uint64_t t_ns, uint32_t seq);
};