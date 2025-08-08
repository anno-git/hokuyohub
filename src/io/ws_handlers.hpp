#pragma once
#include <drogon/WebSocketController.h>
#include <memory>
#include <vector>
#include "detect/dbscan.hpp"

class NngBus;

class LiveWs : public drogon::WebSocketController<LiveWs> {
  NngBus& bus_;
public:
  explicit LiveWs(NngBus& b):bus_(b){}
  void handleNewMessage(const drogon::WebSocketConnectionPtr&, std::string&&, const drogon::WebSocketMessageType&) override;
  void pushClustersLite(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items);
  WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws/live", drogon::Get);
  WS_PATH_LIST_END
};