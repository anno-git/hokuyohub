#include "osc_publisher.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <vector>
#include <cmath>
#include <chrono>

#ifdef USE_OSC
  #ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    // Windows doesn't have ssize_t, define it for compatibility
    typedef intptr_t ssize_t;
  #else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
  #endif
#endif

// ===== Helper note =====
// OSC Bundle format:
//   "#bundle" + '\0' (OSC-string, 4-byte padded)
//   64-bit NTP timetag
//   [ for each element:
//       int32 size (big-endian) of the following message bytes
//       raw OSC message bytes
//   ]
// We convert unix t_ns -> NTP (secs since 1900 + fractional 32-bit).

OscPublisher::OscPublisher() : enabled_(false), rate_limit_(0) {
#ifdef USE_OSC
  socket_fd_ = -1;
#ifdef _WIN32
  // Initialize Winsock on Windows
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    std::cerr << "[OscPublisher] Failed to initialize Winsock" << std::endl;
  }
#endif
#endif
}

OscPublisher::~OscPublisher() {
  stop();
#ifdef USE_OSC
#ifdef _WIN32
  // Clean up Winsock on Windows
  WSACleanup();
#endif
#endif
}

void OscPublisher::start(const SinkConfig& config) {
  if (!config.isOsc()) return;

  // Parse URL like "osc://host:port/path"
  std::string url = config.osc().url;
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
  // TODO: map localhost to 127.0.0.1
  rate_limit_ = config.rate_limit;
  bundle_fragment_size_ = config.osc().bundle_fragment_size;
  in_bundle_ = config.osc().in_bundle;

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
#ifdef _WIN32
    closesocket(socket_fd_);
#else
    close(socket_fd_);
#endif
    socket_fd_ = -1;
    enabled_ = false;
    return;
  }

  enabled_ = true;
#endif
}

void OscPublisher::stop() {
#ifdef USE_OSC
  if (socket_fd_ >= 0) {
#ifdef _WIN32
    closesocket(socket_fd_);
#else
    close(socket_fd_);
#endif
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

// ---- NEW: Bundle encoder ----
static inline void write_be32(std::ostringstream& ss, uint32_t v) {
  ss << char((v >> 24) & 0xff) << char((v >> 16) & 0xff)
     << char((v >> 8) & 0xff)  << char(v & 0xff);
}

static inline void write_be64(std::ostringstream& ss, uint64_t v) {
  ss << char((v >> 56) & 0xff) << char((v >> 48) & 0xff)
     << char((v >> 40) & 0xff) << char((v >> 32) & 0xff)
     << char((v >> 24) & 0xff) << char((v >> 16) & 0xff)
     << char((v >> 8) & 0xff)  << char(v & 0xff);
}

// unix ns -> NTP 64-bit
static inline uint64_t unix_ns_to_ntp(uint64_t t_ns) {
  const uint64_t NTP_EPOCH_OFFSET = 2208988800ULL; // seconds between 1900 and 1970
  uint64_t secs = t_ns / 1000000000ULL;
  uint64_t rem_ns = t_ns % 1000000000ULL;

  uint64_t ntp_secs = secs + NTP_EPOCH_OFFSET;
  // fractional = (rem_ns / 1e9) * 2^32
  long double frac = (long double)rem_ns / 1000000000.0L;
  uint64_t ntp_frac = (uint64_t)std::llround(frac * 4294967296.0L); // 2^32

  return (ntp_secs << 32) | (ntp_frac & 0xffffffffULL);
}

static inline void pad4(std::ostringstream& ss) {
  while (ss.tellp() % 4 != 0) ss << '\0';
}

std::string OscPublisher::encodeOscMessage(const std::string& address, uint32_t id, uint64_t t_ns, uint32_t seq,
                                           float cx, float cy, float minx, float miny, float maxx, float maxy, uint32_t n) {
  std::ostringstream ss;

  // Address
  ss << address << '\0';
  pad4(ss);

  // Type tag string (i,t,i,f,f,f,f,f,f,i) => ",itiffffffi"
  ss << ",ihiffffffi" << '\0';
  pad4(ss);

  // Writers
  auto writeInt32 = [&](uint32_t val) { write_be32(ss, val); };
  auto writeInt64 = [&](uint64_t val) { write_be64(ss, val); };
  auto writeFloat32 = [&](float val) {
    union { float f; uint32_t i; } u;
    u.f = val;
    write_be32(ss, u.i);
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

std::string OscPublisher::encodeOscBundle(const std::vector<std::string>& messages, uint64_t t_ns) {
  std::ostringstream ss;

  // "#bundle" OSC-string
  ss << "#bundle" << '\0';
  pad4(ss); // (#bundle + \0 は 8B なので既に4揃え)

  // NTP timetag
  uint64_t ntp = unix_ns_to_ntp(t_ns);
  write_be64(ss, ntp);

  // Each element: size (int32 BE) + message bytes
  for (const auto& msg : messages) {
    write_be32(ss, static_cast<uint32_t>(msg.size()));
    ss.write(msg.data(), static_cast<std::streamsize>(msg.size()));
  }

  return ss.str();
}

void OscPublisher::publishClusters(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items) {
  if (!enabled_ || !shouldPublish()) return;

  // 1) まず全メッセージを生成
  std::vector<std::string> msgs;
  msgs.reserve(items.size());
  for (const auto& c : items) {
    msgs.emplace_back(encodeOscMessage(path_, c.id, t_ns, seq,
                                       c.cx, c.cy,
                                       c.minx, c.miny,
                                       c.maxx, c.maxy,
                                       static_cast<uint32_t>(c.point_indices.size())));
  }

  if(in_bundle_) {
    // 2) UDP MTU 対策で適度にチャンク分割して Bundle 送信
    //    だいたい 1200B を目安（ヘッダ 16B + 各要素の size(4B) を見込む）
    const size_t SOFT_UDP_LIMIT = bundle_fragment_size_;
    std::vector<std::string> chunk;
    size_t current_bytes = 16; // "#bundle"+timetag = 8+8 = 16B

    auto flush_chunk = [&](bool force) {
        if (chunk.empty() && !force) return;
        std::string bundle = encodeOscBundle(chunk, t_ns);
        sendUdp(bundle);
        chunk.clear();
        current_bytes = 16;
    };

    for (const auto& m : msgs) {
        size_t add_bytes = 4 + m.size(); // size field + message
        if (!chunk.empty() && SOFT_UDP_LIMIT > 0 && (current_bytes + add_bytes) > SOFT_UDP_LIMIT) {
            flush_chunk(false);
        }
        chunk.push_back(m);
        current_bytes += add_bytes;
    }
    flush_chunk(true);
  }
  else {
    for (const auto& m : msgs) {
      sendUdp(m);
    }
  }
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