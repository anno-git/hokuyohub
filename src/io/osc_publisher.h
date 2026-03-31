#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include "detect/dbscan.h"
#include "config/config.h"

#ifdef USE_OSC
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#endif

class OscPublisher {
  std::string host_;
  int port_;
  std::string path_;
  bool enabled_{false};
  int rate_limit_{0};
  bool in_bundle_{false};
  uint64_t bundle_fragment_size_{0};
  std::string data_type_{"cluster"};
  std::chrono::steady_clock::time_point last_publish_;
  // Separate timestamp for raw-point publishes so clusters and raw don't share rate limit
  std::chrono::steady_clock::time_point last_publish_raw_;
  
#ifdef USE_OSC
  int socket_fd_{-1};
  struct sockaddr_in addr_;
#endif

public:
  OscPublisher();
  ~OscPublisher();
  
  void start(const SinkConfig& config);
  void publishClusters(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items);
  void publishRaw(uint64_t t_ns, uint32_t seq, const std::vector<float>& xy, const std::vector<uint8_t>& sid);
  void stop();
  
  bool isEnabled() const { return enabled_; }
  
private:
  std::string encodeOscBundle(const std::vector<std::string>& messages, uint64_t t_ns);
  std::string encodeOscMessage(const std::string& address, uint32_t id, uint64_t t_ns, uint32_t seq, 
                              float cx, float cy, float minx, float miny, float maxx, float maxy, uint32_t n);
  std::string encodeOscStringMessage(const std::string& address, const std::string& s);
  std::string encodeOscPointMessage(const std::string& address, uint64_t t_ns, uint32_t seq, float x, float y, uint32_t sid);
  bool shouldPublish();
  bool shouldPublishRaw();
  void sendUdp(const std::string& data);
};