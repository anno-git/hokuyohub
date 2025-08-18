#pragma once
#include <string>
#include <vector>
#include "core/mask.h"

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
  std::string listen{"0.0.0.0:8080"};
};

struct NngConfig {
  std::string url{"tcp://0.0.0.0:5555"};
  std::string encoding{"msgpack"};
};

struct OscConfig {
  std::string url{"osc://0.0.0.0:7000/hokuyohub/cluster"};
  bool in_bundle{false}; // Use OSC bundle for multiple messages
  uint64_t bundle_fragment_size{0}; // Fragment size for OSC bundle
};

struct SinkConfig {
  std::string topic{"clusters"};
  int         rate_limit{0};

  std::variant<OscConfig, NngConfig> cfg{OscConfig{}};

  bool isOsc() const { return std::holds_alternative<OscConfig>(cfg); }
  bool isNng() const { return std::holds_alternative<NngConfig>(cfg); }
  OscConfig& osc() { return std::get<OscConfig>(cfg); }
  NngConfig& nng() { return std::get<NngConfig>(cfg); }
  const OscConfig& osc() const { return std::get<OscConfig>(cfg); }
  const NngConfig& nng() const { return std::get<NngConfig>(cfg); }
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

struct PrefilterConfig {
    bool enabled{true};
    
    // Strategy 1: Neighborhood count filter
    struct {
        bool enabled{true};
        int k{5};                    // Minimum neighbors required
        float r_base{0.05f};         // Base radius [m]
        float r_scale{1.0f};         // Distance-based radius scaling factor
    } neighborhood;
    
    // Strategy 2: Angular derivative spike removal
    struct {
        bool enabled{true};
        float dr_threshold{0.3f};    // |dR/dθ| threshold for spike detection
        int window_size{3};          // Window size for derivative calculation
    } spike_removal;
    
    // Strategy 3: Moving median/robust regression outlier removal
    struct {
        bool enabled{true};
        int median_window{5};        // Window size for moving median
        float outlier_threshold{2.0f}; // Threshold in standard deviations
        bool use_robust_regression{false}; // Use robust regression instead of median
    } outlier_removal;
    
    // Strategy 4: Sensor intensity/reliability filtering
    struct {
        bool enabled{false};         // Disabled by default (sensor dependent)
        float min_intensity{0.0f};   // Minimum intensity threshold
        float min_reliability{0.0f}; // Minimum reliability threshold
    } intensity_filter;
    
    // Strategy 5: Post-processing cluster isolation removal
    struct {
        bool enabled{true};
        int min_cluster_size{3};     // Minimum points to keep a cluster
        float isolation_radius{0.1f}; // Radius to check for isolation
    } isolation_removal;
};

struct PostfilterConfig {
    bool enabled{true};
    
    // Strategy 1: Cluster isolation removal
    struct {
        bool enabled{true};
        int min_points_size{3};     // Minimum points to keep a cluster
        float isolation_radius{0.1f}; // Radius to check for isolation between clusters
        int required_neighbors{2};     // Minimum neighbors required for a point to be considered non-isolated
    } isolation_removal;
    
    // Future strategies can be added here
    // struct {
    //     bool enabled{false};
    //     // parameters...
    // } future_strategy;
};

struct SecurityConfig {
  std::string api_token; // empty => auth disabled
};

struct AppConfig {
  std::vector<SensorConfig> sensors;

  // Legacy fields for backward compatibility
  float dbscan_eps{0.12f};
  int dbscan_minPts{6};
  
  DbscanConfig dbscan{};
  PrefilterConfig prefilter{};
  PostfilterConfig postfilter{};
  UiConfig ui{};
  std::vector<SinkConfig> sinks;
  SecurityConfig security{};
  core::WorldMask world_mask{};
};

AppConfig load_app_config(const std::string& path);
std::string dump_app_config(const AppConfig& cfg);