#pragma once
#include <crow.h>
#include "core/sensor_manager.h"
#include "core/filter_manager.h"
#include "detect/dbscan.h"
#include "io/publisher_manager.h"
#include "config/config.h"
#include "ws_handlers.h"
#include <memory>

class RestApi {
   SensorManager& sensors_;
   FilterManager& filters_;
   DBSCAN2D& dbscan_;
   PublisherManager& publisher_manager_;
   std::shared_ptr<LiveWs> ws_;
   AppConfig& config_;
   std::string token_;

  public:
    RestApi(SensorManager& s, FilterManager& f, DBSCAN2D& d, PublisherManager& pm, std::shared_ptr<LiveWs> w, AppConfig& cfg)
     : sensors_(s), filters_(f), dbscan_(d), publisher_manager_(pm), ws_(std::move(w)), config_(cfg), token_(cfg.security.api_token) {}

    // Register all routes with the Crow app
    void registerRoutes(crow::SimpleApp& app);

  // Sink runtime management (public for main.cpp access)
  void applySinksRuntime();

private:
  bool authorize(const crow::request& req) const;
  crow::response sendUnauthorized() const;

  // Sensors
  crow::response getSensors();
  crow::response getSensorById(const std::string& id);
  crow::response patchSensor(const std::string& id, const crow::request& req);
  
  // Filters
  crow::response getPrefilter();
  crow::response putPrefilter(const crow::request& req);
  crow::response getPostfilter();
  crow::response putPostfilter(const crow::request& req);
  crow::response getFilters();
  
  // DBSCAN
  crow::response getDbscan();
  crow::response putDbscan(const crow::request& req);
  
  // Sensors (new endpoints)
  crow::response postSensor(const crow::request& req);
  crow::response deleteSensor(const std::string& id);
  
  // Sinks
  crow::response getSinks();
  crow::response postSink(const crow::request& req);
  crow::response patchSink(int index, const crow::request& req);
  crow::response deleteSink(int index);
  
  // Snapshot
  crow::response getSnapshot();
  
  // Configs
  crow::response getConfigsList(const crow::request& req);
  crow::response postConfigsLoad(const crow::request& req);
  crow::response postConfigsImport(const crow::request& req);
  crow::response postConfigsSave(const crow::request& req);
  crow::response getConfigsExport();
};