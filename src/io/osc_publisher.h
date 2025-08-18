#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include "detect/dbscan.h"
#include "config/config.h"

#ifdef USE_OSC
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

class OscPublisher {
  std::string host_;
  int port_;
  std::string path_;
  bool enabled_{false};
  int rate_limit_{0};
  std::chrono::steady_clock::time_point last_publish_;
  
#ifdef USE_OSC
  int socket_fd_{-1};
  struct sockaddr_in addr_;
#endif

public:
  OscPublisher();
  ~OscPublisher();
  
  void start(const SinkConfig& config);
  void publishClusters(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items);
  void stop();
  
  bool isEnabled() const { return enabled_; }
  
private:
  std::string encodeOscMessage(const std::string& address, uint32_t id, uint64_t t_ns, uint32_t seq, 
                              float cx, float cy, float minx, float miny, float maxx, float maxy, uint32_t n);
  bool shouldPublish();
  void sendUdp(const std::string& data);
};