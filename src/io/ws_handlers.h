#pragma once
#include <crow.h>
#include <json/json.h>
#include <memory>
#include <vector>
#include <mutex>
#include <unordered_set>
#include "detect/dbscan.h"

class PublisherManager;
class SensorManager; // 追加: 前方宣言
class FilterManager; // 追加: FilterManager前方宣言
struct AppConfig; // 追加: AppConfig前方宣言

class LiveWs {
   PublisherManager& publisher_manager_;
   SensorManager* sensorManager_{nullptr};
   FilterManager* filterManager_{nullptr};
   AppConfig* appConfig_{nullptr};
   DBSCAN2D* dbscan_{nullptr};
 public:
   explicit LiveWs(PublisherManager& pm) : publisher_manager_(pm) {}

   void setSensorManager(SensorManager* sm) { sensorManager_ = sm; }
   void setFilterManager(FilterManager* fm) { filterManager_ = fm; }
   void setAppConfig(AppConfig* cfg) { appConfig_ = cfg; }
   void setDbscan(DBSCAN2D* dbscan) { dbscan_ = dbscan; }

   // Register WebSocket routes with the Crow app
   void registerWebSocketRoutes(crow::SimpleApp& app);

   // WebSocket event handlers (now private, called from route handlers)
   void handleNewConnection(crow::websocket::connection& conn);
   void handleConnectionClosed(crow::websocket::connection& conn, const std::string& reason);
   void handleNewMessage(crow::websocket::connection& conn, const std::string& data, bool is_binary);

   // 全接続へ通知
   static void broadcast(std::string_view msg);
   void pushClustersLite(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items);
   void pushRawLite(uint64_t t_ns, uint32_t seq, const std::vector<float>& xy, const std::vector<uint8_t>& sid);
   void pushFilteredLite(uint64_t t_ns, uint32_t seq, const std::vector<float>& xy, const std::vector<uint8_t>& sid);

   // 追加: センサー状態の送受信用ユーティリティ
   Json::Value buildSnapshot() const;
   void sendSnapshotTo(crow::websocket::connection& conn);
   void broadcastSnapshot(); // Broadcast snapshot to all connected clients
   void broadcastSensorUpdated(std::string sensor_id);
   void handleSensorUpdate(crow::websocket::connection& conn, const Json::Value& j);
   void handleFilterUpdate(crow::websocket::connection& conn, const Json::Value& j);
   void handleWorldUpdate(crow::websocket::connection& conn, const Json::Value& j);
   
   // 追加: フィルター設定の送受信用ユーティリティ
   void broadcastFilterConfigUpdate();
   void sendFilterConfigTo(crow::websocket::connection& conn);
   
   // 追加: DBSCAN設定の送受信用ユーティリティ
   void sendDbscanConfigTo(crow::websocket::connection& conn);
   void handleDbscanUpdate(crow::websocket::connection& conn, const Json::Value& j);
   
   // 追加: センサー追加用ユーティリティ
   void handleSensorAdd(crow::websocket::connection& conn, const Json::Value& j);
   
   // 追加: Sink設定の送受信用ユーティリティ
   void handleSinkAdd(crow::websocket::connection& conn, const Json::Value& j);
   void handleSinkUpdate(crow::websocket::connection& conn, const Json::Value& j);
   void handleSinkDelete(crow::websocket::connection& conn, const Json::Value& j);

 private:
   static std::mutex mtx_;
   static std::unordered_set<crow::websocket::connection*> conns_;
};
