#pragma once
#include <string>
#include <vector>

struct SensorMaskLocal { float angle_min, angle_max; float range_near, range_far; };
struct Pose { float tx, ty, theta; };
struct SensorCfg {
  int id; std::string name; std::string transport; std::string endpoint; bool enabled; Pose pose; SensorMaskLocal mask;
};

struct AppConfig {
  std::vector<SensorCfg> sensors;
  float dbscan_eps{0.12f}; int dbscan_minPts{6};
};

AppConfig load_app_config(const std::string& path);
