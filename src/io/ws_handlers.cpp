#include "ws_handlers.hpp"
#include <drogon/drogon.h>

void LiveWs::handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& msg, const drogon::WebSocketMessageType& type){
  if(type==drogon::WebSocketMessageType::Text){
    // echo
    conn->send(msg);
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
  for(auto& conn : drogon::app().getWSSessions("/ws/live")){
    conn->send(j.toStyledString());
  }
}