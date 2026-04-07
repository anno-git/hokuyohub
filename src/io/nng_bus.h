#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include "detect/dbscan.h"
#include "config/config.h"
#ifdef USE_NNG
#include <nng/nng.h>
#endif
class NngBus {
  std::string url_;
  std::string encoding_{"msgpack"}; // Default to msgpack
  bool enabled_{false};
  bool send_clusters_{true};
  bool send_raw_{false};
  std::string cluster_topic_;
  std::string raw_topic_;
  
#ifdef USE_NNG
  nng_socket socket_;
  bool socket_initialized_{false};
#endif

public:
  NngBus();
  ~NngBus();
  
  void startPublisher(const std::string& url);
  void startPublisher(const SinkConfig& config);
  void updateConfig(const SinkConfig& config);
  void publishClusters(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items);
  void publishRaw(uint64_t t_ns, uint32_t seq, const std::vector<float>& xy, const std::vector<uint8_t>& sid);
  void stop();
  
  bool isEnabled() const { return enabled_; }
  
private:
  std::string serializeToMessagePack(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items);
  std::string serializeToJson(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items);
  std::string serializeRawToMessagePack(uint64_t t_ns, uint32_t seq, const std::vector<float>& xy, const std::vector<uint8_t>& sid);
  std::string serializeRawToJson(uint64_t t_ns, uint32_t seq, const std::vector<float>& xy, const std::vector<uint8_t>& sid);
};