#include "ws_handlers.h"
#include <drogon/drogon.h>

std::mutex LiveWs::mtx_;
std::unordered_set<drogon::WebSocketConnectionPtr> LiveWs::conns_;

void LiveWs::handleNewConnection(const drogon::HttpRequestPtr&,
                                 const drogon::WebSocketConnectionPtr& conn){
  std::lock_guard<std::mutex> lk(mtx_);
  conns_.insert(conn);
}

void LiveWs::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn){
  std::lock_guard<std::mutex> lk(mtx_);
  conns_.erase(conn);
}

void LiveWs::handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                              std::string&& msg,
                              const drogon::WebSocketMessageType& type){
  if(type==drogon::WebSocketMessageType::Text){
    conn->send(msg); // echo
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