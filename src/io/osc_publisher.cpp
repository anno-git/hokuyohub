#include "osc_publisher.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>

OscPublisher::OscPublisher() : enabled_(false), rate_limit_(0) {
#ifdef USE_OSC
  socket_fd_ = -1;
#endif
}

OscPublisher::~OscPublisher() {
  stop();
}

void OscPublisher::start(const SinkConfig& config) {
  if (config.type != "osc") return;
  
  // Parse URL like "osc://host:port/path"
  std::string url = config.url;
  if (url.find("osc://") == 0) {
    url = url.substr(6); // Remove "osc://"
  }
  
  size_t slash_pos = url.find('/');
  std::string host_port = (slash_pos != std::string::npos) ? url.substr(0, slash_pos) : url;
  path_ = (slash_pos != std::string::npos) ? url.substr(slash_pos) : "/hokuyohub/cluster";
  
  size_t colon_pos = host_port.find(':');
  if (colon_pos != std::string::npos) {
    host_ = host_port.substr(0, colon_pos);
    port_ = std::stoi(host_port.substr(colon_pos + 1));
  } else {
    host_ = host_port;
    port_ = 7000; // Default OSC port
  }
  
  rate_limit_ = config.rate_limit;
  
#ifdef USE_OSC
  socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_fd_ < 0) {
    std::cerr << "[OscPublisher] Failed to create UDP socket" << std::endl;
    enabled_ = false;
    return;
  }
  
  memset(&addr_, 0, sizeof(addr_));
  addr_.sin_family = AF_INET;
  addr_.sin_port = htons(port_);
  
  if (inet_pton(AF_INET, host_.c_str(), &addr_.sin_addr) <= 0) {
    std::cerr << "[OscPublisher] Invalid host address: " << host_ << std::endl;
    close(socket_fd_);
    socket_fd_ = -1;
    enabled_ = false;
    return;
  }
  
  enabled_ = true;
  std::cout << "[OscPublisher] Started on " << host_ << ":" << port_ << path_ 
            << " with rate_limit=" << rate_limit_ << "Hz" << std::endl;
#else
  std::cout << "[OscPublisher] OSC support not compiled, publisher disabled" << std::endl;
  enabled_ = false;
#endif
}

void OscPublisher::stop() {
#ifdef USE_OSC
  if (socket_fd_ >= 0) {
    close(socket_fd_);
    socket_fd_ = -1;
  }
#endif
  enabled_ = false;
}

bool OscPublisher::shouldPublish() {
  if (rate_limit_ <= 0) return true;
  
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_publish_).count();
  auto min_interval = 1000 / rate_limit_; // ms
  
  if (elapsed >= min_interval) {
    last_publish_ = now;
    return true;
  }
  
  return false;
}

void OscPublisher::publishClusters(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items) {
  if (!enabled_ || !shouldPublish()) return;
  
  // Send each cluster as a separate OSC message to avoid MTU issues
  for (const auto& cluster : items) {
    std::string message = encodeOscMessage(path_, cluster.id, t_ns, seq,
                                         cluster.cx, cluster.cy,
                                         cluster.minx, cluster.miny,
                                         cluster.maxx, cluster.maxy,
                                         static_cast<uint32_t>(cluster.point_indices.size()));
    sendUdp(message);
  }
}

std::string OscPublisher::encodeOscMessage(const std::string& address, uint32_t id, uint64_t t_ns, uint32_t seq,
                                          float cx, float cy, float minx, float miny, float maxx, float maxy, uint32_t n) {
  std::ostringstream ss;
  
  // OSC Address Pattern (null-terminated, padded to 4-byte boundary)
  ss << address;
  ss << '\0';
  while (ss.tellp() % 4 != 0) {
    ss << '\0';
  }
  
  // OSC Type Tag String: ,ititfffffff (i=int32, t=timetag, f=float32)
  ss << ",ititfffffff";
  ss << '\0';
  while (ss.tellp() % 4 != 0) {
    ss << '\0';
  }
  
  // Arguments in network byte order (big-endian)
  auto writeInt32 = [&](uint32_t val) {
    ss << char((val >> 24) & 0xff);
    ss << char((val >> 16) & 0xff);
    ss << char((val >> 8) & 0xff);
    ss << char(val & 0xff);
  };
  
  auto writeInt64 = [&](uint64_t val) {
    ss << char((val >> 56) & 0xff);
    ss << char((val >> 48) & 0xff);
    ss << char((val >> 40) & 0xff);
    ss << char((val >> 32) & 0xff);
    ss << char((val >> 24) & 0xff);
    ss << char((val >> 16) & 0xff);
    ss << char((val >> 8) & 0xff);
    ss << char(val & 0xff);
  };
  
  auto writeFloat32 = [&](float val) {
    union { float f; uint32_t i; } u;
    u.f = val;
    writeInt32(u.i);
  };
  
  // Arguments: id, t_ns, seq, cx, cy, minx, miny, maxx, maxy, n
  writeInt32(id);
  writeInt64(t_ns);
  writeInt32(seq);
  writeFloat32(cx);
  writeFloat32(cy);
  writeFloat32(minx);
  writeFloat32(miny);
  writeFloat32(maxx);
  writeFloat32(maxy);
  writeInt32(n);
  
  return ss.str();
}

void OscPublisher::sendUdp(const std::string& data) {
#ifdef USE_OSC
  if (socket_fd_ < 0) return;
  
  ssize_t sent = sendto(socket_fd_, data.c_str(), data.size(), 0,
                       (struct sockaddr*)&addr_, sizeof(addr_));
  if (sent < 0) {
    std::cerr << "[OscPublisher] Failed to send UDP packet" << std::endl;
  }
#endif
}