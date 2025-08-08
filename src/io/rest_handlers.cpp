#include "rest_handlers.h"
#include <drogon/drogon.h>

void RestApi::getSensors(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&cb){
  auto resp = drogon::HttpResponse::newHttpJsonResponse(Json::Value(Json::arrayValue));
  cb(resp);
}
