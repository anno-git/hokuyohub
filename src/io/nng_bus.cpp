#include "nng_bus.h"
#ifdef USE_NNG
#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>
#endif
#include <json/json.h>
#include <iostream>
#include <sstream>
#include <iomanip>

NngBus::NngBus() : enabled_(false), rate_limit_(0) {
#ifdef USE_NNG
  socket_initialized_ = false;
#endif
}

NngBus::~NngBus() {
  stop();
}

void NngBus::startPublisher(const std::string& url) {
  url_ = url;
  enabled_ = !url.empty();
  
  // TODO: map localhost to 127.0.0.1 if necessary

#ifdef USE_NNG
  if (!enabled_) return;
  
  int rv = nng_pub0_open(&socket_);
  if (rv != 0) {
    std::cerr << "[NngBus] Failed to open pub socket: " << nng_strerror(rv) << std::endl;
    enabled_ = false;
    return;
  }
  socket_initialized_ = true;
  
  rv = nng_listen(socket_, url_.c_str(), nullptr, 0);
  if (rv != 0) {
    std::cerr << "[NngBus] Failed to listen on " << url_ << ": " << nng_strerror(rv) << std::endl;
    nng_close(socket_);
    socket_initialized_ = false;
    enabled_ = false;
    return;
  }
#endif
}

void NngBus::startPublisher(const SinkConfig& config) {
  if (!config.isNng()) return;

  url_ = config.nng().url;
  encoding_ = config.nng().encoding.empty() ? "msgpack" : config.nng().encoding;
  rate_limit_ = config.rate_limit;
  send_clusters_ = config.send_clusters;
  send_raw_ = config.send_raw;
  cluster_topic_ = config.cluster_topic;
  raw_topic_ = config.raw_topic;
  enabled_ = !url_.empty();
  
#ifdef USE_NNG
  if (!enabled_) return;
  
  int rv = nng_pub0_open(&socket_);
  if (rv != 0) {
    std::cerr << "[NngBus] Failed to open pub socket: " << nng_strerror(rv) << std::endl;
    enabled_ = false;
    return;
  }
  socket_initialized_ = true;
  
  rv = nng_listen(socket_, url_.c_str(), nullptr, 0);
  if (rv != 0) {
    std::cerr << "[NngBus] Failed to listen on " << url_ << ": " << nng_strerror(rv) << std::endl;
    nng_close(socket_);
    socket_initialized_ = false;
    enabled_ = false;
    return;
  }
#endif
}

void NngBus::stop() {
#ifdef USE_NNG
  if (socket_initialized_) {
    nng_close(socket_);
    socket_initialized_ = false;
  }
#endif
  enabled_ = false;
}

bool NngBus::shouldPublish() {
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

void NngBus::publishClusters(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items) {
  if (!enabled_ || !send_clusters_ || !shouldPublish()) return;
  
#ifdef USE_NNG
  std::string data;
  
  // Use the configured encoding
  if (encoding_ == "json") {
    data = serializeToJson(t_ns, seq, items);
  } else {
    // Default to MessagePack, with JSON fallback on error
    try {
      data = serializeToMessagePack(t_ns, seq, items);
    } catch (const std::exception& e) {
      std::cerr << "[NngBus] MessagePack serialization failed, using JSON: " << e.what() << std::endl;
      data = serializeToJson(t_ns, seq, items);
    }
  }
  
  if (data.empty()) return;

  // Prepend topic prefix for NNG pub/sub filtering
  std::string payload;
  if (!cluster_topic_.empty()) {
    payload = cluster_topic_ + data;
  } else {
    payload = data;
  }

  nng_msg* msg;
  int rv = nng_msg_alloc(&msg, payload.size());
  if (rv != 0) {
    std::cerr << "[NngBus] Failed to allocate message: " << nng_strerror(rv) << std::endl;
    return;
  }

  memcpy(nng_msg_body(msg), payload.c_str(), payload.size());

  rv = nng_sendmsg(socket_, msg, NNG_FLAG_NONBLOCK);
  if (rv != 0) {
    std::cerr << "[NngBus] Failed to send message: " << nng_strerror(rv) << std::endl;
    nng_msg_free(msg);
  }
#endif
}

std::string NngBus::serializeToMessagePack(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items) {
  // Simple MessagePack implementation for basic types
  // This is a minimal implementation - in production you'd use a proper MessagePack library
  std::ostringstream ss;
  
  // MessagePack map with 5 elements: {v, seq, t_ns, items, raw}
  ss << char(0x85); // fixmap with 5 elements
  
  // "v": 1
  ss << char(0xa1) << 'v'; // fixstr with 1 char
  ss << char(0x01); // positive fixint 1
  
  // "seq": seq
  ss << char(0xa3) << "seq"; // fixstr with 3 chars
  if (seq < 128) {
    ss << char(seq); // positive fixint
  } else {
    ss << char(0xcc) << char(seq & 0xff); // uint8
  }
  
  // "t_ns": t_ns
  ss << char(0xa4) << "t_ns"; // fixstr with 4 chars
  ss << char(0xcf); // uint64
  for (int i = 7; i >= 0; i--) {
    ss << char((t_ns >> (i * 8)) & 0xff);
  }
  
  // "items": array of clusters
  ss << char(0xa5) << "items"; // fixstr with 5 chars
  if (items.size() < 16) {
    ss << char(0x90 | items.size()); // fixarray
  } else {
    ss << char(0xdc); // array16
    ss << char((items.size() >> 8) & 0xff);
    ss << char(items.size() & 0xff);
  }
  
  for (const auto& cluster : items) {
    // Each cluster is a map with 9 elements: {id, cx, cy, minx, miny, maxx, maxy, n}
    ss << char(0x88); // fixmap with 8 elements
    
    // "id": cluster.id
    ss << char(0xa2) << "id"; // fixstr with 2 chars
    if (cluster.id < 128) {
      ss << char(cluster.id); // positive fixint
    } else {
      ss << char(0xcc) << char(cluster.id & 0xff); // uint8
    }
    
    // "cx": cluster.cx
    ss << char(0xa2) << "cx";
    ss << char(0xcb); // float64
    union { double d; uint64_t i; } u;
    u.d = cluster.cx;
    for (int i = 7; i >= 0; i--) {
      ss << char((u.i >> (i * 8)) & 0xff);
    }
    
    // "cy": cluster.cy
    ss << char(0xa2) << "cy";
    ss << char(0xcb); // float64
    u.d = cluster.cy;
    for (int i = 7; i >= 0; i--) {
      ss << char((u.i >> (i * 8)) & 0xff);
    }
    
    // "minx": cluster.minx
    ss << char(0xa4) << "minx";
    ss << char(0xcb); // float64
    u.d = cluster.minx;
    for (int i = 7; i >= 0; i--) {
      ss << char((u.i >> (i * 8)) & 0xff);
    }
    
    // "miny": cluster.miny
    ss << char(0xa4) << "miny";
    ss << char(0xcb); // float64
    u.d = cluster.miny;
    for (int i = 7; i >= 0; i--) {
      ss << char((u.i >> (i * 8)) & 0xff);
    }
    
    // "maxx": cluster.maxx
    ss << char(0xa4) << "maxx";
    ss << char(0xcb); // float64
    u.d = cluster.maxx;
    for (int i = 7; i >= 0; i--) {
      ss << char((u.i >> (i * 8)) & 0xff);
    }
    
    // "maxy": cluster.maxy
    ss << char(0xa4) << "maxy";
    ss << char(0xcb); // float64
    u.d = cluster.maxy;
    for (int i = 7; i >= 0; i--) {
      ss << char((u.i >> (i * 8)) & 0xff);
    }
    
    // "n": cluster.point_indices.size()
    ss << char(0xa1) << 'n';
    uint32_t n = static_cast<uint32_t>(cluster.point_indices.size());
    if (n < 128) {
      ss << char(n); // positive fixint
    } else if (n < 256) {
      ss << char(0xcc) << char(n & 0xff); // uint8
    } else {
      ss << char(0xcd); // uint16
      ss << char((n >> 8) & 0xff);
      ss << char(n & 0xff);
    }
  }
  
  // "raw": false
  ss << char(0xa3) << "raw"; // fixstr with 3 chars
  ss << char(0xc2); // false
  
  return ss.str();
}

std::string NngBus::serializeToJson(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items) {
  Json::Value root;
  root["v"] = 1;
  root["seq"] = seq;
  root["t_ns"] = Json::UInt64(t_ns);
  root["raw"] = false;
  
  Json::Value items_array(Json::arrayValue);
  for (const auto& cluster : items) {
    Json::Value item;
    item["id"] = cluster.id;
    item["cx"] = cluster.cx;
    item["cy"] = cluster.cy;
    item["minx"] = cluster.minx;
    item["miny"] = cluster.miny;
    item["maxx"] = cluster.maxx;
    item["maxy"] = cluster.maxy;
    item["n"] = static_cast<int>(cluster.point_indices.size());
    items_array.append(item);
  }
  root["items"] = items_array;
  
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  return Json::writeString(builder, root);
}

void NngBus::publishRaw(uint64_t t_ns, uint32_t seq, const std::vector<float>& xy, const std::vector<uint8_t>& sid) {
  if (!enabled_ || !send_raw_ || !shouldPublish()) return;

#ifdef USE_NNG
  std::string data;

  if (encoding_ == "json") {
    data = serializeRawToJson(t_ns, seq, xy, sid);
  } else {
    try {
      data = serializeRawToMessagePack(t_ns, seq, xy, sid);
    } catch (const std::exception& e) {
      std::cerr << "[NngBus] MessagePack raw serialization failed, using JSON: " << e.what() << std::endl;
      data = serializeRawToJson(t_ns, seq, xy, sid);
    }
  }

  if (data.empty()) return;

  // Prepend topic prefix for NNG pub/sub filtering
  std::string payload;
  if (!raw_topic_.empty()) {
    payload = raw_topic_ + data;
  } else {
    payload = data;
  }

  nng_msg* msg;
  int rv = nng_msg_alloc(&msg, payload.size());
  if (rv != 0) {
    std::cerr << "[NngBus] Failed to allocate message: " << nng_strerror(rv) << std::endl;
    return;
  }

  memcpy(nng_msg_body(msg), payload.c_str(), payload.size());

  rv = nng_sendmsg(socket_, msg, NNG_FLAG_NONBLOCK);
  if (rv != 0) {
    std::cerr << "[NngBus] Failed to send raw message: " << nng_strerror(rv) << std::endl;
    nng_msg_free(msg);
  }
#endif
}

std::string NngBus::serializeRawToMessagePack(uint64_t t_ns, uint32_t seq, const std::vector<float>& xy, const std::vector<uint8_t>& sid) {
  std::ostringstream ss;
  size_t npoints = xy.size() / 2;

  // MessagePack map with 5 elements: {v, seq, t_ns, raw, points}
  ss << char(0x85);

  // "v": 1
  ss << char(0xa1) << 'v';
  ss << char(0x01);

  // "seq": seq
  ss << char(0xa3) << "seq";
  if (seq < 128) {
    ss << char(seq);
  } else {
    ss << char(0xce); // uint32
    for (int i = 3; i >= 0; i--) ss << char((seq >> (i * 8)) & 0xff);
  }

  // "t_ns": t_ns
  ss << char(0xa4) << "t_ns";
  ss << char(0xcf); // uint64
  for (int i = 7; i >= 0; i--) ss << char((t_ns >> (i * 8)) & 0xff);

  // "raw": true
  ss << char(0xa3) << "raw";
  ss << char(0xc3); // true

  // "points": array of {x, y, sid}
  ss << char(0xa6) << "points";
  if (npoints < 16) {
    ss << char(0x90 | npoints);
  } else if (npoints < 65536) {
    ss << char(0xdc);
    ss << char((npoints >> 8) & 0xff);
    ss << char(npoints & 0xff);
  } else {
    ss << char(0xdd);
    for (int i = 3; i >= 0; i--) ss << char((npoints >> (i * 8)) & 0xff);
  }

  for (size_t i = 0; i < npoints; ++i) {
    // Each point is a map with 3 elements: {x, y, sid}
    ss << char(0x83);

    // "x": float64
    ss << char(0xa1) << 'x';
    ss << char(0xcb);
    union { double d; uint64_t u; } ux;
    ux.d = xy[i * 2];
    for (int j = 7; j >= 0; j--) ss << char((ux.u >> (j * 8)) & 0xff);

    // "y": float64
    ss << char(0xa1) << 'y';
    ss << char(0xcb);
    ux.d = xy[i * 2 + 1];
    for (int j = 7; j >= 0; j--) ss << char((ux.u >> (j * 8)) & 0xff);

    // "sid": uint8
    ss << char(0xa3) << "sid";
    uint8_t s = (i < sid.size()) ? sid[i] : 0;
    ss << char(s); // positive fixint (0-255 fits in uint8, but fixint is 0-127)
  }

  return ss.str();
}

std::string NngBus::serializeRawToJson(uint64_t t_ns, uint32_t seq, const std::vector<float>& xy, const std::vector<uint8_t>& sid) {
  Json::Value root;
  root["v"] = 1;
  root["seq"] = seq;
  root["t_ns"] = Json::UInt64(t_ns);
  root["raw"] = true;

  Json::Value points_array(Json::arrayValue);
  size_t npoints = xy.size() / 2;
  for (size_t i = 0; i < npoints; ++i) {
    Json::Value pt;
    pt["x"] = xy[i * 2];
    pt["y"] = xy[i * 2 + 1];
    pt["sid"] = (i < sid.size()) ? static_cast<int>(sid[i]) : 0;
    points_array.append(pt);
  }
  root["points"] = points_array;

  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  return Json::writeString(builder, root);
}
