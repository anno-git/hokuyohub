#include "ws_handlers.h"
#include <drogon/drogon.h>
#include "core/sensor_manager.h"  // ★ SensorManager へ橋渡し

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
    o["minx"]=c.minx; o["miny"]=c.miny; o["maxx"]=c.maxx; o["maxy"]=c.maxy; o["count"]=c.count; o["sensor_mask"]=c.sensor_mask;
    j["items"].append(o);
  }
  // 全接続にブロードキャスト
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