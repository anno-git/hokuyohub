#pragma once
#include <drogon/WebSocketController.h>
#include <memory>
#include <vector>
#include <mutex>
#include <unordered_set>
#include "detect/dbscan.h"

class NngBus;
class SensorManager; // 追加: 前方宣言
class FilterManager; // 追加: FilterManager前方宣言
struct AppConfig; // 追加: AppConfig前方宣言

class LiveWs : public drogon::WebSocketController<LiveWs, false> { // ★ AutoCreationを無効化
   NngBus& bus_;
   SensorManager* sensorManager_{nullptr};
   FilterManager* filterManager_{nullptr};
   AppConfig* appConfig_{nullptr};
 public:
   explicit LiveWs(NngBus& b) : bus_(b) {}

   void setSensorManager(SensorManager* sm) { sensorManager_ = sm; }
   void setFilterManager(FilterManager* fm) { filterManager_ = fm; }
   void setAppConfig(AppConfig* cfg) { appConfig_ = cfg; }

   void handleNewConnection(const drogon::HttpRequestPtr&,
                            const drogon::WebSocketConnectionPtr&) override;
   void handleConnectionClosed(const drogon::WebSocketConnectionPtr&) override;
   void handleNewMessage(const drogon::WebSocketConnectionPtr&,
                         std::string&&,
                         const drogon::WebSocketMessageType&) override;

   // 全接続へ通知
   static void broadcast(std::string_view msg);
   void pushClustersLite(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items);
   void pushRawLite(uint64_t t_ns, uint32_t seq, const std::vector<float>& xy, const std::vector<uint8_t>& sid);
   void pushFilteredLite(uint64_t t_ns, uint32_t seq, const std::vector<float>& xy, const std::vector<uint8_t>& sid);

   // 追加: センサー状態の送受信用ユーティリティ
   void sendSnapshotTo(const drogon::WebSocketConnectionPtr& conn);
   void broadcastSensorUpdated(int id);
   void handleSensorUpdate(const drogon::WebSocketConnectionPtr& conn, const Json::Value& j);
   void handleFilterUpdate(const drogon::WebSocketConnectionPtr& conn, const Json::Value& j);
   void handleWorldUpdate(const drogon::WebSocketConnectionPtr& conn, const Json::Value& j);

   WS_PATH_LIST_BEGIN
     WS_PATH_ADD("/ws/live", drogon::Get);
   WS_PATH_LIST_END

 private:
   static std::mutex mtx_;
   static std::unordered_set<drogon::WebSocketConnectionPtr> conns_;
};
