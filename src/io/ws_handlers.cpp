#include "ws_handlers.h"
#include <drogon/drogon.h>
#include "core/sensor_manager.h"  // ★ SensorManager へ橋渡し
#include "core/filter_manager.h"  // ★ FilterManager へ橋渡し
#include "config/config.h"        // ★ AppConfig へ橋渡し

std::mutex LiveWs::mtx_;
std::unordered_set<drogon::WebSocketConnectionPtr> LiveWs::conns_;

void LiveWs::handleNewConnection(const drogon::HttpRequestPtr&,
                                 const drogon::WebSocketConnectionPtr& conn){
  std::lock_guard<std::mutex> lk(mtx_);
  conns_.insert(conn);
  // 接続直後に snapshot を送る（サーバ主導、クライアントのRefresh不要）
  sendSnapshotTo(conn);
}

void LiveWs::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn){
  std::lock_guard<std::mutex> lk(mtx_);
  conns_.erase(conn);
}

void LiveWs::handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                              std::string&& msg,
                              const drogon::WebSocketMessageType& type){
  if(type==drogon::WebSocketMessageType::Text){
    // 既存: 不明メッセージは echo。まずは JSON として解釈を試みる
    Json::CharReaderBuilder b;
    std::unique_ptr<Json::CharReader> r(b.newCharReader());
    Json::Value j;
    std::string errs;
    bool ok = r->parse(msg.data(), msg.data()+msg.size(), &j, &errs);
    if(!ok || !j.isObject()){
      conn->send(msg); // パースできなければ従来通り echo
      return;
    }

    const auto t = j.get("type","").asString();
    // ---- 追加: 状態/操作メッセージ ----
    if(t == "sensor.requestSnapshot"){
      sendSnapshotTo(conn);
      return;
    }
    if(t == "sensor.enable"){
      const int id = j.get("id",-1).asInt();
      const bool en = j.get("enabled",true).asBool();
      bool applied = false;
      if(sensorManager_){
        applied = sensorManager_->setEnabled(id, en);
      }
      Json::Value res;
      if(applied){
        res["type"] = "ok"; res["ref"] = "sensor.enable";
        conn->send(res.toStyledString());
        broadcastSensorUpdated(id);
      }else{
        res["type"]="error"; res["ref"]="sensor.enable"; res["message"]="invalid id";
        conn->send(res.toStyledString());
      }
      return;
    }
    if(t == "sensor.update"){
      handleSensorUpdate(conn, j);
      return;
    }
    if(t == "filter.update"){
      handleFilterUpdate(conn, j);
      return;
    }
    if(t == "world.update"){
      handleWorldUpdate(conn, j);
      return;
    }
    // -----------------------------------

    // 既存互換: 上記に該当しない Text は echo（後方互換）
    conn->send(msg);
  }
}

void LiveWs::broadcast(std::string_view msg){
  std::lock_guard<std::mutex> lk(mtx_);
  for(const auto& c : conns_){
    if(c && c->connected()) c->send(std::string{msg});
  }
}

void LiveWs::pushClustersLite(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items){
  Json::Value j; j["type"]="clusters-lite"; j["t"] = Json::UInt64(t_ns); j["seq"] = Json::UInt(seq);
  j["items"] = Json::arrayValue;
  for(const auto& c: items){
      Json::Value o; o["id"]=Json::UInt(c.id); o["cx"]=c.cx; o["cy"]=c.cy;
      o["minx"]=c.minx; o["miny"]=c.miny; o["maxx"]=c.maxx; o["maxy"]=c.maxy; o["count"]=(int)c.point_indices.size(); o["sensor_mask"]=c.sensor_mask;
      j["items"].append(o);
  }
  // 全接続にブロードキャスト
  LiveWs::broadcast(j.toStyledString());
}

void LiveWs::pushRawLite(uint64_t t_ns, uint32_t seq, const std::vector<float>& xy, const std::vector<uint8_t>& sid){
  Json::Value j;
  j["type"] = "raw-lite";
  j["t"] = Json::UInt64(t_ns);
  j["seq"] = Json::UInt(seq);
  
  // Convert xy vector to JSON array
  j["xy"] = Json::arrayValue;
  for(const auto& val : xy){
    j["xy"].append(val);
  }
  
  // Convert sid vector to JSON array
  j["sid"] = Json::arrayValue;
  for(const auto& val : sid){
    j["sid"].append(Json::UInt(val));
  }
  
  // Broadcast to all connections
  LiveWs::broadcast(j.toStyledString());
}

void LiveWs::pushFilteredLite(uint64_t t_ns, uint32_t seq, const std::vector<float>& xy, const std::vector<uint8_t>& sid){
  Json::Value j;
  j["type"] = "filtered-lite";
  j["t"] = Json::UInt64(t_ns);
  j["seq"] = Json::UInt(seq);
  
  // Convert xy vector to JSON array
  j["xy"] = Json::arrayValue;
  for(const auto& val : xy){
    j["xy"].append(val);
  }
  
  // Convert sid vector to JSON array
  j["sid"] = Json::arrayValue;
  for(const auto& val : sid){
    j["sid"].append(Json::UInt(val));
  }
  
  // Broadcast to all connections
  LiveWs::broadcast(j.toStyledString());
}

// ==== 追加: センサー状態の送受信用ユーティリティ ====
void LiveWs::sendSnapshotTo(const drogon::WebSocketConnectionPtr& conn){
  Json::Value out; out["type"]="sensor.snapshot";
  // SensorManager から JSON（jsoncpp）でもらう想定。無ければ空配列。
  if(sensorManager_){
    out["sensors"] = sensorManager_->listAsJson(); // 例: [{id,enabled,...}, ...]
  }else{
    out["sensors"] = Json::arrayValue;
  }
  
  // Add world mask data to snapshot
  if(appConfig_){
    out["world_mask"]["includes"] = Json::arrayValue;
    out["world_mask"]["excludes"] = Json::arrayValue;
    
    // Convert include polygons to JSON
    for (const auto& polygon : appConfig_->world_mask.include) {
      Json::Value poly_json = Json::arrayValue;
      for (const auto& point : polygon.points) {
        Json::Value point_json = Json::arrayValue;
        point_json.append(point.x);
        point_json.append(point.y);
        poly_json.append(point_json);
      }
      out["world_mask"]["includes"].append(poly_json);
    }
    
    // Convert exclude polygons to JSON
    for (const auto& polygon : appConfig_->world_mask.exclude) {
      Json::Value poly_json = Json::arrayValue;
      for (const auto& point : polygon.points) {
        Json::Value point_json = Json::arrayValue;
        point_json.append(point.x);
        point_json.append(point.y);
        poly_json.append(point_json);
      }
      out["world_mask"]["excludes"].append(poly_json);
    }
  }
  
  if(conn && conn->connected()){
    conn->send(out.toStyledString());
  }
  std::cout << out.toStyledString() << std::endl;
}

void LiveWs::broadcastSensorUpdated(int id){
  Json::Value out; out["type"]="sensor.updated";
  if(sensorManager_){
    out["sensor"] = sensorManager_->getAsJson(id); // 例: {id,enabled,...}
  }else{
    Json::Value s; s["id"]=id; s["enabled"]=false; out["sensor"]=s;
  }
  const auto payload = out.toStyledString();
  std::lock_guard<std::mutex> lk(mtx_);
  for(const auto& c : conns_){
    if(c && c->connected()) c->send(payload);
  }
}

void LiveWs::handleSensorUpdate(const drogon::WebSocketConnectionPtr& conn, const Json::Value& j){
  const int id = j.get("id",-1).asInt();
  const Json::Value patch = j.get("patch", Json::Value(Json::objectValue));

  if(!sensorManager_){
    Json::Value res; res["type"]="error"; res["ref"]="sensor.update"; res["message"]="sensorManager not set";
    conn->send(res.toStyledString());
    return;
  }

  Json::Value applied; std::string err;
  if(sensorManager_->applyPatch(id, patch, applied, err)){
    Json::Value res; res["type"]="ok"; res["ref"]="sensor.update";
    res["applied"]=applied;
    res["sensor"]=sensorManager_->getAsJson(id);
    conn->send(res.toStyledString());
    broadcastSensorUpdated(id);
  }else{
    Json::Value res; res["type"]="error"; res["ref"]="sensor.update"; res["message"]=err;
    conn->send(res.toStyledString());
  }
}

void LiveWs::handleWorldUpdate(const drogon::WebSocketConnectionPtr& conn, const Json::Value& j){
  const Json::Value patch = j.get("patch", Json::Value(Json::objectValue));
  
  Json::Value res;
  res["type"] = "ok";
  res["ref"] = "world.update";
  
  if (!appConfig_) {
    res["type"] = "error";
    res["message"] = "AppConfig not available";
    conn->send(res.toStyledString());
    return;
  }
  
  // Extract world_mask from patch
  if (!patch.isMember("world_mask")) {
    res["type"] = "error";
    res["message"] = "Missing world_mask in patch";
    conn->send(res.toStyledString());
    return;
  }
  
  const Json::Value world_mask = patch["world_mask"];
  
  try {
    // Clear existing world mask
    appConfig_->world_mask.include.clear();
    appConfig_->world_mask.exclude.clear();
    
    // Parse include regions
    if (world_mask.isMember("includes") && world_mask["includes"].isArray()) {
      for (const auto& polygon_json : world_mask["includes"]) {
        if (polygon_json.isArray()) {
          core::Polygon polygon;
          for (const auto& point_json : polygon_json) {
            if (point_json.isArray() && point_json.size() >= 2) {
              double x = point_json[0].asDouble();
              double y = point_json[1].asDouble();
              polygon.points.emplace_back(x, y);
            }
          }
          if (!polygon.points.empty()) {
            appConfig_->world_mask.include.push_back(std::move(polygon));
          }
        }
      }
    }
    
    // Parse exclude regions
    if (world_mask.isMember("excludes") && world_mask["excludes"].isArray()) {
      for (const auto& polygon_json : world_mask["excludes"]) {
        if (polygon_json.isArray()) {
          core::Polygon polygon;
          for (const auto& point_json : polygon_json) {
            if (point_json.isArray() && point_json.size() >= 2) {
              double x = point_json[0].asDouble();
              double y = point_json[1].asDouble();
              polygon.points.emplace_back(x, y);
            }
          }
          if (!polygon.points.empty()) {
            appConfig_->world_mask.exclude.push_back(std::move(polygon));
          }
        }
      }
    }
    
    res["message"] = "World mask updated successfully";
    std::cout << "[WorldUpdate] World mask updated successfully. Include regions: " 
              << appConfig_->world_mask.include.size() 
              << ", Exclude regions: " << appConfig_->world_mask.exclude.size() << std::endl;
    
    // Broadcast the update to all connected clients
    Json::Value broadcast_msg;
    broadcast_msg["type"] = "world.updated";
    broadcast_msg["world_mask"]["includes"] = Json::arrayValue;
    broadcast_msg["world_mask"]["excludes"] = Json::arrayValue;
    
    // Convert back to JSON for broadcasting
    for (const auto& polygon : appConfig_->world_mask.include) {
      Json::Value poly_json = Json::arrayValue;
      for (const auto& point : polygon.points) {
        Json::Value point_json = Json::arrayValue;
        point_json.append(point.x);
        point_json.append(point.y);
        poly_json.append(point_json);
      }
      broadcast_msg["world_mask"]["includes"].append(poly_json);
    }
    
    for (const auto& polygon : appConfig_->world_mask.exclude) {
      Json::Value poly_json = Json::arrayValue;
      for (const auto& point : polygon.points) {
        Json::Value point_json = Json::arrayValue;
        point_json.append(point.x);
        point_json.append(point.y);
        poly_json.append(point_json);
      }
      broadcast_msg["world_mask"]["excludes"].append(poly_json);
    }
    
    broadcast(broadcast_msg.toStyledString());
    
  } catch (const std::exception& e) {
    res["type"] = "error";
    res["message"] = std::string("Failed to update world mask: ") + e.what();
    std::cout << "[WorldUpdate] Failed to update world mask: " << e.what() << std::endl;
  }
  
  conn->send(res.toStyledString());
}

void LiveWs::handleFilterUpdate(const drogon::WebSocketConnectionPtr& conn, const Json::Value& j){
  const Json::Value config = j.get("config", Json::Value(Json::objectValue));
  
  Json::Value res;
  res["type"] = "ok";
  res["ref"] = "filter.update";
  
  if (!filterManager_) {
    res["type"] = "error";
    res["message"] = "FilterManager not available";
    conn->send(res.toStyledString());
    return;
  }
  
  // Update filter configuration through FilterManager
  bool success = filterManager_->updateFilterConfig(config);
  
  if (success) {
    res["message"] = "Filter configuration updated successfully";
    std::cout << "[FilterUpdate] Filter configuration updated successfully" << std::endl;
  } else {
    res["type"] = "error";
    res["message"] = "Failed to update filter configuration";
    std::cout << "[FilterUpdate] Failed to update filter configuration" << std::endl;
  }
  
  conn->send(res.toStyledString());
}