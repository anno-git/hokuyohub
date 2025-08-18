#pragma once
#include <drogon/HttpController.h>
#include "core/sensor_manager.h"
#include "core/filter_manager.h"
#include "detect/dbscan.h"
#include "io/nng_bus.h"
#include "config/config.h"
#include "ws_handlers.h"
#include <memory>

class RestApi : public drogon::HttpController<RestApi, false> { // ★ AutoCreationを無効化
   SensorManager& sensors_;
   FilterManager& filters_;
   DBSCAN2D& dbscan_;
   NngBus& bus_;
   std::shared_ptr<LiveWs> ws_;
   AppConfig& config_;
   std::string token_;

  public:
    RestApi(SensorManager& s, FilterManager& f, DBSCAN2D& d, NngBus& b, std::shared_ptr<LiveWs> w, AppConfig& cfg)
     : sensors_(s), filters_(f), dbscan_(d), bus_(b), ws_(std::move(w)), config_(cfg), token_(cfg.security.api_token) {}

  METHOD_LIST_BEGIN
    // Sensors
    ADD_METHOD_TO(RestApi::getSensors, "/api/v1/sensors", drogon::Get);
    ADD_METHOD_TO(RestApi::getSensorById, "/api/v1/sensors/{id}", drogon::Get);
    ADD_METHOD_TO(RestApi::patchSensor, "/api/v1/sensors/{id}", drogon::Patch);
    
    // Filters
    ADD_METHOD_TO(RestApi::getPrefilter, "/api/v1/filters/prefilter", drogon::Get);
    ADD_METHOD_TO(RestApi::putPrefilter, "/api/v1/filters/prefilter", drogon::Put);
    ADD_METHOD_TO(RestApi::getPostfilter, "/api/v1/filters/postfilter", drogon::Get);
    ADD_METHOD_TO(RestApi::putPostfilter, "/api/v1/filters/postfilter", drogon::Put);
    ADD_METHOD_TO(RestApi::getFilters, "/api/v1/filters", drogon::Get);
    
    // DBSCAN
    ADD_METHOD_TO(RestApi::getDbscan, "/api/v1/dbscan", drogon::Get);
    ADD_METHOD_TO(RestApi::putDbscan, "/api/v1/dbscan", drogon::Put);
    
    // Sensors (new endpoints)
    ADD_METHOD_TO(RestApi::postSensor, "/api/v1/sensors", drogon::Post);
    ADD_METHOD_TO(RestApi::deleteSensor, "/api/v1/sensors/{id}", drogon::Delete);
    
    // Sinks
    ADD_METHOD_TO(RestApi::getSinks, "/api/v1/sinks", drogon::Get);
    ADD_METHOD_TO(RestApi::postSink, "/api/v1/sinks", drogon::Post);
    ADD_METHOD_TO(RestApi::patchSink, "/api/v1/sinks/{index}", drogon::Patch);
    ADD_METHOD_TO(RestApi::deleteSink, "/api/v1/sinks/{index}", drogon::Delete);
    
    // Snapshot
    ADD_METHOD_TO(RestApi::getSnapshot, "/api/v1/snapshot", drogon::Get);
    
    // Configs
    ADD_METHOD_TO(RestApi::getConfigsList, "/api/v1/configs/list", drogon::Get);
    ADD_METHOD_TO(RestApi::postConfigsLoad, "/api/v1/configs/load", drogon::Post);
    ADD_METHOD_TO(RestApi::postConfigsImport, "/api/v1/configs/import", drogon::Post);
    ADD_METHOD_TO(RestApi::postConfigsSave, "/api/v1/configs/save", drogon::Post);
    ADD_METHOD_TO(RestApi::getConfigsExport, "/api/v1/configs/export", drogon::Get);
  METHOD_LIST_END

private:
  bool authorize(const drogon::HttpRequestPtr& req) const;
  void sendUnauthorized(std::function<void (const drogon::HttpResponsePtr &)>&& callback) const;

  // Sensors
  void getSensors(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  void getSensorById(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  void patchSensor(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  
  // Filters
  void getPrefilter(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  void putPrefilter(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  void getPostfilter(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  void putPostfilter(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  void getFilters(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  
  // DBSCAN
  void getDbscan(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  void putDbscan(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  
  // Sensors (new endpoints)
  void postSensor(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  void deleteSensor(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  
  // Sinks
  void getSinks(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  void postSink(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  void patchSink(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  void deleteSink(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  
  // Snapshot
  void getSnapshot(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  
  // Configs
  void getConfigsList(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  void postConfigsLoad(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  void postConfigsImport(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  void postConfigsSave(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
  void getConfigsExport(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&);
};