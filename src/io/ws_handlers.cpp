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
      // WebSocket uses slot index (numeric) for sensor operations
      const std::string sensor_id = j.get("id",-1).asString();
      const bool en = j.get("enabled",true).asBool();
      bool applied = false;
      if(sensorManager_){
        applied = sensorManager_->setEnabled(sensor_id, en);
      }
      Json::Value res;
      if(applied){
        res["type"] = "ok"; res["ref"] = "sensor.enable";
        conn->send(res.toStyledString());
        broadcastSensorUpdated(sensor_id);
      }else{
        res["type"]="error"; res["ref"]="sensor.enable"; res["message"]="invalid sensor id";
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
    if(t == "filter.requestConfig"){
      sendFilterConfigTo(conn);
      return;
    }
    if(t == "dbscan.requestConfig"){
      sendDbscanConfigTo(conn);
      return;
    }
    if(t == "dbscan.update"){
      handleDbscanUpdate(conn, j);
      return;
    }
    if(t == "sensor.add"){
      handleSensorAdd(conn, j);
      return;
    }
    if(t == "sink.add"){
      handleSinkAdd(conn, j);
      return;
    }
    if(t == "sink.update"){
      handleSinkUpdate(conn, j);
      return;
    }
    if(t == "sink.delete"){
      handleSinkDelete(conn, j);
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

Json::Value LiveWs::buildSnapshot() const
{
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
  
  // Add filter configuration to snapshot
  if(filterManager_){
    out["filter_config"] = filterManager_->getFilterConfigAsJson();
  }
  
  // Add publishers/sinks information to snapshot
  if(appConfig_){
    Json::Value publishers;
    Json::Value sinks_array = Json::arrayValue;
    bool nng_enabled = false;
    bool osc_enabled = false;
    
    // Build new sinks array format
    for (const auto& sink : appConfig_->sinks) {
      Json::Value sink_obj;
      sink_obj["enabled"] = true; // Assume enabled if in config
      if (sink.isNng()) {
        const auto& cfg = sink.nng();
        sink_obj["type"] = "nng";
        sink_obj["url"] = cfg.url;
        sink_obj["encoding"] = cfg.encoding;
      }
      else if (sink.isOsc()) {
        const auto& cfg = sink.osc();
        sink_obj["type"] = "osc";
        sink_obj["url"] = cfg.url;
        sink_obj["in_bundle"] = cfg.in_bundle;
        sink_obj["bundle_fragment_size"] = cfg.bundle_fragment_size;
      }
      if (!sink.topic.empty()) {
        sink_obj["topic"] = sink.topic;
      }
      sink_obj["rate_limit"] = sink.rate_limit;
      
      sinks_array.append(sink_obj);
    }
    
    // Add both formats
    publishers["sinks"] = sinks_array;
    out["publishers"] = publishers;
  }
  return out;
}

// ==== 追加: センサー状態の送受信用ユーティリティ ====
void LiveWs::sendSnapshotTo(const drogon::WebSocketConnectionPtr& conn){
  Json::Value out = buildSnapshot();
  if(conn && conn->connected()){
    conn->send(out.toStyledString());
  }
}

void LiveWs::broadcastSnapshot(){
  Json::Value out = buildSnapshot();
  const auto payload = out.toStyledString();
  std::lock_guard<std::mutex> lk(mtx_);
  for(const auto& c : conns_){
    if(c && c->connected()) c->send(payload);
  }
  std::cout << "[LiveWs] Broadcasted snapshot to " << conns_.size() << " clients" << std::endl;
}

void LiveWs::broadcastSensorUpdated(std::string sensor_id){
  Json::Value out; out["type"]="sensor.updated";
  if(sensorManager_){
    out["sensor"] = sensorManager_->getAsJson(sensor_id);
  }else{
    Json::Value s; s["id"]=sensor_id; s["enabled"]=false; out["sensor"]=s;
  }
  const auto payload = out.toStyledString();
  std::lock_guard<std::mutex> lk(mtx_);
  for(const auto& c : conns_){
    if(c && c->connected()) c->send(payload);
  }
}

void LiveWs::handleSensorUpdate(const drogon::WebSocketConnectionPtr& conn, const Json::Value& j){
  // WebSocket uses slot index (numeric) for sensor operations
  std::string sensor_id = j.get("id", "").asString();
  const Json::Value patch = j.get("patch", Json::Value(Json::objectValue));

  if(!sensorManager_){
    Json::Value res; res["type"]="error"; res["ref"]="sensor.update"; res["message"]="sensorManager not set";
    conn->send(res.toStyledString());
    return;
  }

  Json::Value applied; std::string err;
  if(sensorManager_->applyPatch(sensor_id, patch, applied, err)){
    Json::Value res; res["type"]="ok"; res["ref"]="sensor.update";
    res["applied"]=applied;
    res["sensor"]=sensorManager_->getAsJson(sensor_id);
    conn->send(res.toStyledString());
    broadcastSensorUpdated(sensor_id);
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
    
    // Broadcast filter configuration update to all clients
    broadcastFilterConfigUpdate();
  } else {
    res["type"] = "error";
    res["message"] = "Failed to update filter configuration";
    std::cout << "[FilterUpdate] Failed to update filter configuration" << std::endl;
  }
  
  conn->send(res.toStyledString());
}

void LiveWs::broadcastFilterConfigUpdate(){
  if (!filterManager_) return;
  
  Json::Value out;
  out["type"] = "filter.updated";
  out["config"] = filterManager_->getFilterConfigAsJson();
  
  const auto payload = out.toStyledString();
  std::lock_guard<std::mutex> lk(mtx_);
  for(const auto& c : conns_){
    if(c && c->connected()) c->send(payload);
  }
  std::cout << "[LiveWs] Broadcasted filter config update to " << conns_.size() << " clients" << std::endl;
}

void LiveWs::sendFilterConfigTo(const drogon::WebSocketConnectionPtr& conn){
  if (!filterManager_ || !conn || !conn->connected()) return;
  
  Json::Value out;
  out["type"] = "filter.config";
  out["config"] = filterManager_->getFilterConfigAsJson();
  
  conn->send(out.toStyledString());
}

void LiveWs::sendDbscanConfigTo(const drogon::WebSocketConnectionPtr& conn){
  if (!appConfig_ || !conn || !conn->connected()) return;
  
  Json::Value out;
  out["type"] = "dbscan.config";
  out["config"]["eps_norm"] = appConfig_->dbscan.eps_norm;
  out["config"]["minPts"] = appConfig_->dbscan.minPts;
  out["config"]["k_scale"] = appConfig_->dbscan.k_scale;
  out["config"]["h_min"] = appConfig_->dbscan.h_min;
  out["config"]["h_max"] = appConfig_->dbscan.h_max;
  out["config"]["R_max"] = appConfig_->dbscan.R_max;
  out["config"]["M_max"] = appConfig_->dbscan.M_max;
  
  conn->send(out.toStyledString());
}

void LiveWs::handleDbscanUpdate(const drogon::WebSocketConnectionPtr& conn, const Json::Value& j){
  const Json::Value config = j.get("config", Json::Value(Json::objectValue));
  
  Json::Value res;
  res["type"] = "ok";
  res["ref"] = "dbscan.update";
  
  if (!appConfig_) {
    res["type"] = "error";
    res["message"] = "AppConfig not available";
    conn->send(res.toStyledString());
    return;
  }
  
  try {
    bool updated = false;
    
    if (config.isMember("eps_norm")) {
      float eps_norm = config["eps_norm"].asFloat();
      if (eps_norm < 0.1f || eps_norm > 10.0f) {
        res["type"] = "error";
        res["message"] = "eps_norm must be between 0.1 and 10.0";
        conn->send(res.toStyledString());
        return;
      }
      appConfig_->dbscan.eps_norm = eps_norm;
      updated = true;
    }
    
    if (config.isMember("minPts")) {
      int minPts = config["minPts"].asInt();
      if (minPts < 1 || minPts > 100) {
        res["type"] = "error";
        res["message"] = "minPts must be between 1 and 100";
        conn->send(res.toStyledString());
        return;
      }
      appConfig_->dbscan.minPts = minPts;
      updated = true;
    }
    
    if (config.isMember("k_scale")) {
      float k_scale = config["k_scale"].asFloat();
      if (k_scale < 0.1f || k_scale > 10.0f) {
        res["type"] = "error";
        res["message"] = "k_scale must be between 0.1 and 10.0";
        conn->send(res.toStyledString());
        return;
      }
      appConfig_->dbscan.k_scale = k_scale;
      updated = true;
    }
    
    if (config.isMember("h_min")) {
      float h_min = config["h_min"].asFloat();
      if (h_min < 0.001f || h_min > appConfig_->dbscan.h_max) {
        res["type"] = "error";
        res["message"] = "h_min must be between 0.001 and h_max";
        conn->send(res.toStyledString());
        return;
      }
      appConfig_->dbscan.h_min = h_min;
      updated = true;
    }
    
    if (config.isMember("h_max")) {
      float h_max = config["h_max"].asFloat();
      if (h_max < appConfig_->dbscan.h_min || h_max > 1.0f) {
        res["type"] = "error";
        res["message"] = "h_max must be between h_min and 1.0";
        conn->send(res.toStyledString());
        return;
      }
      appConfig_->dbscan.h_max = h_max;
      updated = true;
    }
    
    if (config.isMember("R_max")) {
      int R_max = config["R_max"].asInt();
      if (R_max < 1 || R_max > 50) {
        res["type"] = "error";
        res["message"] = "R_max must be between 1 and 50";
        conn->send(res.toStyledString());
        return;
      }
      appConfig_->dbscan.R_max = R_max;
      updated = true;
    }
    
    if (config.isMember("M_max")) {
      int M_max = config["M_max"].asInt();
      if (M_max < 10 || M_max > 5000) {
        res["type"] = "error";
        res["message"] = "M_max must be between 10 and 5000";
        conn->send(res.toStyledString());
        return;
      }
      appConfig_->dbscan.M_max = M_max;
      updated = true;
    }
    
    if (updated) {
      res["message"] = "DBSCAN configuration updated successfully";
      
      // Broadcast the update to all connected clients
      Json::Value broadcast_msg;
      broadcast_msg["type"] = "dbscan.updated";
      broadcast_msg["config"]["eps_norm"] = appConfig_->dbscan.eps_norm;
      broadcast_msg["config"]["minPts"] = appConfig_->dbscan.minPts;
      broadcast_msg["config"]["k_scale"] = appConfig_->dbscan.k_scale;
      broadcast_msg["config"]["h_min"] = appConfig_->dbscan.h_min;
      broadcast_msg["config"]["h_max"] = appConfig_->dbscan.h_max;
      broadcast_msg["config"]["R_max"] = appConfig_->dbscan.R_max;
      broadcast_msg["config"]["M_max"] = appConfig_->dbscan.M_max;
      
      broadcast(broadcast_msg.toStyledString());
    } else {
      res["message"] = "No changes made";
    }
    
  } catch (const std::exception& e) {
    res["type"] = "error";
    res["message"] = std::string("Failed to update DBSCAN config: ") + e.what();
  }
  
  conn->send(res.toStyledString());
}

void LiveWs::handleSensorAdd(const drogon::WebSocketConnectionPtr& conn, const Json::Value& j){
  const Json::Value cfg = j.get("cfg", Json::Value(Json::objectValue));
  
  Json::Value res;
  res["type"] = "ok";
  res["ref"] = "sensor.add";
  
  if (!appConfig_ || !sensorManager_) {
    res["type"] = "error";
    res["message"] = "AppConfig or SensorManager not available";
    conn->send(res.toStyledString());
    return;
  }
  
  try {
    // This is a simplified implementation - in practice, you'd want to use the same logic as the REST API
    res["message"] = "Sensor addition via WebSocket not fully implemented - use REST API";
    res["type"] = "error";
    
  } catch (const std::exception& e) {
    res["type"] = "error";
    res["message"] = std::string("Failed to add sensor: ") + e.what();
  }
  
  conn->send(res.toStyledString());
}

void LiveWs::handleSinkAdd(const drogon::WebSocketConnectionPtr& conn, const Json::Value& j){
  const Json::Value cfg = j.get("cfg", Json::Value(Json::objectValue));
  
  Json::Value res;
  res["type"] = "ok";
  res["ref"] = "sink.add";
  
  if (!appConfig_) {
    res["type"] = "error";
    res["message"] = "AppConfig not available";
    conn->send(res.toStyledString());
    return;
  }
  
  try {
    // This is a simplified implementation - in practice, you'd want to use the same logic as the REST API
    res["message"] = "Sink addition via WebSocket not fully implemented - use REST API";
    res["type"] = "error";
    
  } catch (const std::exception& e) {
    res["type"] = "error";
    res["message"] = std::string("Failed to add sink: ") + e.what();
  }
  
  conn->send(res.toStyledString());
}

void LiveWs::handleSinkUpdate(const drogon::WebSocketConnectionPtr& conn, const Json::Value& j){
  const Json::Value patch = j.get("patch", Json::Value(Json::objectValue));
  const int index = j.get("index", -1).asInt();
  
  Json::Value res;
  res["type"] = "ok";
  res["ref"] = "sink.update";
  
  if (!appConfig_) {
    res["type"] = "error";
    res["message"] = "AppConfig not available";
    conn->send(res.toStyledString());
    return;
  }
  
  try {
    // This is a simplified implementation - in practice, you'd want to use the same logic as the REST API
    res["message"] = "Sink update via WebSocket not fully implemented - use REST API";
    res["type"] = "error";
    
  } catch (const std::exception& e) {
    res["type"] = "error";
    res["message"] = std::string("Failed to update sink: ") + e.what();
  }
  
  conn->send(res.toStyledString());
}

void LiveWs::handleSinkDelete(const drogon::WebSocketConnectionPtr& conn, const Json::Value& j){
  const int index = j.get("index", -1).asInt();
  
  Json::Value res;
  res["type"] = "ok";
  res["ref"] = "sink.delete";
  
  if (!appConfig_) {
    res["type"] = "error";
    res["message"] = "AppConfig not available";
    conn->send(res.toStyledString());
    return;
  }
  
  try {
    // This is a simplified implementation - in practice, you'd want to use the same logic as the REST API
    res["message"] = "Sink deletion via WebSocket not fully implemented - use REST API";
    res["type"] = "error";
    
  } catch (const std::exception& e) {
    res["type"] = "error";
    res["message"] = std::string("Failed to delete sink: ") + e.what();
  }
  
  conn->send(res.toStyledString());
}