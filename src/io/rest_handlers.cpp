#include "rest_handlers.hpp"
#include <drogon/drogon.h>

void RestApi::getSensors(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&cb){
  auto resp = drogon::HttpResponse::newHttpJsonResponse(Json::Value(Json::arrayValue));
  cb(resp);
}
