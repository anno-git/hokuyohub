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
  int rate_limit_{0};
  std::chrono::steady_clock::time_point last_publish_;
  
#ifdef USE_NNG
  nng_socket socket_;
  bool socket_initialized_{false};
#endif

public:
  NngBus();
  ~NngBus();
  
  void startPublisher(const std::string& url);
  void startPublisher(const SinkConfig& config);
  void publishClusters(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items);
  void stop();
  
  bool isEnabled() const { return enabled_; }
  
private:
  std::string serializeToMessagePack(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items);
  std::string serializeToJson(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items);
  bool shouldPublish();
};