#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "detect/dbscan.hpp"

class NngBus {
  std::string url_;
public:
  void startPublisher(const std::string& url);
  void publishClusters(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items);
};