#pragma once
#include <string>
#include <vector>

struct PoseDeg {
  float tx{0.0f};
  float ty{0.0f};
  float theta_deg{0.0f}; // degree
};

struct AngleMaskDeg {
  float min_deg{-180.0f};
  float max_deg{ 180.0f};
};

struct RangeMaskM {
  float near_m{0.05f};
  float far_m{15.0f};
};

struct SensorMaskLocal {
  AngleMaskDeg angle{};
  RangeMaskM range{};
};

struct SensorConfig {
  std::string id{""};
  std::string type{"hokuyo_urg_eth"};
  std::string name{"sensor"};
  std::string host{"192.168.1.10"};
  int port{10940};
  bool enabled{true};

  // 取得モードとデバイス設定
  std::string mode{"ME"}; // "MD"=距離のみ, "ME"=距離+強度
  int interval{0};        // ms, 0=既定
  int skip_step{0};
  int ignore_checksum_error{1};

  PoseDeg pose{};
  SensorMaskLocal mask{};
};

struct UiConfig {
  std::string ws_listen{"0.0.0.0:8080"};
  std::string rest_listen{"0.0.0.0:8080"};
};

struct SinkConfig {
  std::string type;      // "nng" など
  std::string url;       // "tcp://0.0.0.0:5555" など
  std::string topic;     // "clusters" など
  std::string encoding;  // "msgpack" など
  int         rate_limit{0};
};

struct DbscanConfig {
  float eps{0.12f};           // Legacy eps (treated as eps_norm)
  float eps_norm{2.5f};       // Normalized distance threshold
  int minPts{5};              // Minimum points for core (inclusive of self)
  float k_scale{1.0f};        // Angular term scale coefficient (1.0 = theoretical optimum)
  
  // Performance parameters
  float h_min{0.01f};         // Minimum grid cell size [m]
  float h_max{0.20f};         // Maximum grid cell size [m]
  int R_max{5};               // Maximum search radius in cells
  int M_max{600};             // Maximum candidate points per query
};

struct AppConfig {
  std::vector<SensorConfig> sensors;

  // Legacy fields for backward compatibility
  float dbscan_eps{0.12f};
  int dbscan_minPts{6};
  
  DbscanConfig dbscan{};
  UiConfig ui{};
  std::vector<SinkConfig> sinks;
};

AppConfig load_app_config(const std::string& path);