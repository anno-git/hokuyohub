#pragma once
#include <drogon/WebSocketController.h>
#include <memory>
#include <vector>
#include <mutex>
#include <unordered_set>
#include "detect/dbscan.h"

class NngBus;

class LiveWs : public drogon::WebSocketController<LiveWs, false> { // ★ AutoCreationを無効化
   NngBus& bus_;
 public:
   explicit LiveWs(NngBus& b) : bus_(b) {}
   void handleNewConnection(const drogon::HttpRequestPtr&,
                            const drogon::WebSocketConnectionPtr&) override;
   void handleConnectionClosed(const drogon::WebSocketConnectionPtr&) override;
   void handleNewMessage(const drogon::WebSocketConnectionPtr&,
                         std::string&&,
                         const drogon::WebSocketMessageType&) override;

   // 全接続へ通知
   static void broadcast(std::string_view msg);
   void pushClustersLite(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items);

   WS_PATH_LIST_BEGIN
     WS_PATH_ADD("/ws/live", drogon::Get);
   WS_PATH_LIST_END

 private:
   static std::mutex mtx_;
   static std::unordered_set<drogon::WebSocketConnectionPtr> conns_;
 };