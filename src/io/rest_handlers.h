#pragma once
#include <drogon/HttpController.h>
#include "core/sensor_manager.h"
#include "detect/dbscan.h"
#include "io/nng_bus.h"
#include <memory>

class LiveWs;

class RestApi : public drogon::HttpController<RestApi, false> { // ★ AutoCreationを無効化
   SensorManager& sensors_; DBSCAN2D& dbscan_; NngBus& bus_; std::shared_ptr<LiveWs> ws_;
  public:
    RestApi(SensorManager& s, DBSCAN2D& d, NngBus& b, std::shared_ptr<LiveWs> w)
     : sensors_(s), dbscan_(d), bus_(b), ws_(std::move(w)) {}
  METHOD_LIST_BEGIN
    ADD_METHOD_TO(RestApi::getSensors, "/api/v1/sensors", drogon::Get);
  METHOD_LIST_END

  void getSensors(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
};