#pragma once

#include <vector>
#include <cstdint>
#include <span>
#include <unordered_map>

struct Cluster { uint32_t id; uint8_t sensor_mask; float cx,cy,minx,miny,maxx,maxy; uint16_t count; };

struct SensorModel {
  float delta_theta_rad; // Angular resolution in radians
  float sigma0;          // Distance noise constant term
  float alpha;           // Distance noise linear coefficient
};

class DBSCAN2D {
  float eps_; int minPts_;
  float k_scale_;        // Angular term scale coefficient (default 1.0)
  std::unordered_map<uint8_t, SensorModel> sensor_models_;
  
  // Performance parameters
  float h_min_, h_max_;  // Grid cell size limits
  int R_max_;            // Maximum search radius in cells
  int M_max_;            // Maximum candidate points per query
  
public:
  // Constructor with default parameters aligned to plan
  DBSCAN2D(float eps, int minPts): eps_(eps), minPts_(minPts), k_scale_(1.0f),
    h_min_(0.01f), h_max_(0.20f), R_max_(5), M_max_(600) {
    // Default sensor model (Δθ=0.25°, σ_r(r)=0.02+0.004·r)
    SensorModel default_model{0.0043633f, 0.02f, 0.004f};
    sensor_models_[0] = default_model; // Default for sid=0
  }
  
  // Parameter setters
  void setParams(float eps, int minPts){ eps_=eps; minPts_=minPts; }
  void setAngularScale(float k_scale) { k_scale_ = k_scale; }
  void setSensorModel(uint8_t sid, float delta_theta_deg, float sigma0, float alpha);
  void setPerformanceParams(float h_min, float h_max, int R_max, int M_max);
  
  // Main clustering function
  // Note: minPts semantics are INCLUSIVE (neighbor count includes the query point itself)
  // k_scale: Angular term scale coefficient (1.0 = theoretical optimum, >1.0 = more tolerant, <1.0 = more strict)
  std::vector<Cluster> run(std::span<const float> xy, std::span<const uint8_t> sid, uint64_t t_ns, uint32_t seq);
};