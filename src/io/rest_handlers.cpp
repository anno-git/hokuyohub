#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00  // Windows 10
#endif
#endif

#include "rest_handlers.h"
#include <json/json.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <regex>
#include <algorithm>

void RestApi::registerRoutes(crow::SimpleApp& app) {
  // Sensors endpoints
  CROW_ROUTE(app, "/api/v1/sensors").methods("GET"_method)([this]() {
    return getSensors();
  });
  
  CROW_ROUTE(app, "/api/v1/sensors/<string>").methods("GET"_method)([this](const std::string& id) {
    return getSensorById(id);
  });
  
  CROW_ROUTE(app, "/api/v1/sensors/<string>").methods("PATCH"_method)([this](const crow::request& req, const std::string& id) {
    return patchSensor(id, req);
  });
  
  CROW_ROUTE(app, "/api/v1/sensors").methods("POST"_method)([this](const crow::request& req) {
    return postSensor(req);
  });
  
  CROW_ROUTE(app, "/api/v1/sensors/<string>").methods("DELETE"_method)([this](const crow::request& req, const std::string& id) {
    if (!authorize(req)) {
      return sendUnauthorized();
    }
    return deleteSensor(id);
  });
  
  // Filters endpoints  
  CROW_ROUTE(app, "/api/v1/filters").methods("GET"_method)([this]() {
    return getFilters();
  });
  
  CROW_ROUTE(app, "/api/v1/filters/prefilter").methods("GET"_method)([this]() {
    return getPrefilter();
  });
  
  CROW_ROUTE(app, "/api/v1/filters/prefilter").methods("PUT"_method)([this](const crow::request& req) {
    return putPrefilter(req);
  });
  
  CROW_ROUTE(app, "/api/v1/filters/postfilter").methods("GET"_method)([this]() {
    return getPostfilter();
  });
  
  CROW_ROUTE(app, "/api/v1/filters/postfilter").methods("PUT"_method)([this](const crow::request& req) {
    return putPostfilter(req);
  });
  
  // DBSCAN endpoints
  CROW_ROUTE(app, "/api/v1/dbscan").methods("GET"_method)([this]() {
    return getDbscan();
  });
  
  CROW_ROUTE(app, "/api/v1/dbscan").methods("PUT"_method)([this](const crow::request& req) {
    return putDbscan(req);
  });
  
  // Sinks endpoints
  CROW_ROUTE(app, "/api/v1/sinks").methods("GET"_method)([this]() {
    return getSinks();
  });
  
  CROW_ROUTE(app, "/api/v1/sinks").methods("POST"_method)([this](const crow::request& req) {
    return postSink(req);
  });
  
  CROW_ROUTE(app, "/api/v1/sinks/<int>").methods("PATCH"_method)([this](const crow::request& req, int index) {
    return patchSink(index, req);
  });
  
  CROW_ROUTE(app, "/api/v1/sinks/<int>").methods("DELETE"_method)([this](const crow::request& req, int index) {
    if (!authorize(req)) {
      return sendUnauthorized();
    }
    return deleteSink(index);
  });
  
  // Snapshot endpoint
  CROW_ROUTE(app, "/api/v1/snapshot").methods("GET"_method)([this]() {
    return getSnapshot();
  });
  
  // Config endpoints
  CROW_ROUTE(app, "/api/v1/configs/list").methods("GET"_method)([this](const crow::request& req) {
    return getConfigsList(req);
  });
  
  CROW_ROUTE(app, "/api/v1/configs/load").methods("POST"_method)([this](const crow::request& req) {
    return postConfigsLoad(req);
  });
  
  CROW_ROUTE(app, "/api/v1/configs/import").methods("POST"_method)([this](const crow::request& req) {
    return postConfigsImport(req);
  });
  
  CROW_ROUTE(app, "/api/v1/configs/save").methods("POST"_method)([this](const crow::request& req) {
    return postConfigsSave(req);
  });
  
  CROW_ROUTE(app, "/api/v1/configs/export").methods("GET"_method)([this]() {
    return getConfigsExport();
  });
}

bool RestApi::authorize(const crow::request& req) const {
  if (token_.empty()) return true; // Auth disabled if no token
  
  auto auth_it = req.headers.find("Authorization");
  if (auth_it == req.headers.end()) return false;
  
  const std::string auth = auth_it->second;
  const std::string bearer_prefix = "Bearer ";
  if (auth.size() <= bearer_prefix.size() || 
      auth.substr(0, bearer_prefix.size()) != bearer_prefix) {
    return false;
  }
  
  return auth.substr(bearer_prefix.size()) == token_;
}

crow::response RestApi::sendUnauthorized() const {
  Json::Value error;
  error["error"] = "unauthorized";
  error["message"] = "Invalid or missing authorization token";
  
  crow::response resp(401, error.toStyledString());
  resp.add_header("Content-Type", "application/json");
  resp.add_header("WWW-Authenticate", "Bearer realm=\"api\", error=\"invalid_token\"");
  return resp;
}

// Sensors endpoints
crow::response RestApi::getSensors() {
  try {
    Json::Value sensors = sensors_.listAsJson();
    crow::response resp(200, sensors.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::getSensorById(const std::string& id) {
  try {
    Json::Value sensor = sensors_.getAsJson(id);

    if (sensor.isNull() || !sensor.isMember("id")) {
      Json::Value error;
      error["error"] = "not_found";
      error["message"] = "Sensor not found";
      crow::response resp(404, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    crow::response resp(200, sensor.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "invalid_id";
    error["message"] = "Invalid sensor slot index";
    crow::response resp(400, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::patchSensor(const std::string& id, const crow::request& req) {
  if (!authorize(req)) {
    return sendUnauthorized();
  }
  
  try {
    Json::CharReaderBuilder builder;
    Json::Value patch;
    std::string errs;
    std::istringstream stream(req.body);
    
    if (!Json::parseFromStream(builder, stream, &patch, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    Json::Value applied;
    std::string err;

    if (sensors_.applyPatch(id, patch, applied, err)) {
      Json::Value result;
      result["id"] = id;
      result["applied"] = applied;
      result["sensor"] = sensors_.getAsJson(id);
      crow::response resp(200, result.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    } else {
      Json::Value error;
      error["error"] = "patch_failed";
      error["message"] = err;
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::postSensor(const crow::request& req) {
  if (!authorize(req)) {
    return sendUnauthorized();
  }
  
  try {
    Json::CharReaderBuilder builder;
    Json::Value sensorData;
    std::string errs;
    std::istringstream stream(req.body);
    
    if (!Json::parseFromStream(builder, stream, &sensorData, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    // Validate required fields
    if (!sensorData.isMember("type") || !sensorData["type"].isString()) {
      Json::Value error;
      error["error"] = "missing_field";
      error["message"] = "Missing required field: type";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    std::string type = sensorData["type"].asString();
    if (type != "hokuyo_urg_eth" && type != "unknown") {
      Json::Value error;
      error["error"] = "invalid_type";
      error["message"] = "Sensor type must be 'hokuyo_urg_eth' or 'unknown'";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    // Generate unique sensor ID
    std::string newId = sensorData.isMember("name") ? sensorData["name"].asString() : "sensor";
    int appendix_max = 0;
    for (const auto& sensor : config_.sensors) {
      if (sensor.id.starts_with(newId)) {
        std::string suffix = sensor.id.substr(newId.length());
        if(suffix.empty()) {
          appendix_max = std::max(appendix_max, 1);
        }
        else if (suffix[0] == ' ') {
          suffix = suffix.substr(1);
          if (std::all_of(suffix.begin(), suffix.end(), ::isdigit)) {
            appendix_max = std::max(appendix_max, std::stoi(suffix) + 1);
          }
        }
      }
    }
    if(appendix_max > 0) {
      newId += " " + std::to_string(appendix_max);
    }
    
    SensorConfig newSensor;
    newSensor.id = newId;
    newSensor.type = type;
    newSensor.name = sensorData.get("name", "New Sensor").asString();
    newSensor.enabled = sensorData.get("enabled", true).asBool();
    
    // Parse endpoint
    if (sensorData.isMember("endpoint")) {
      std::string endpoint = sensorData["endpoint"].asString();
      auto colon_pos = endpoint.find(":");
      if (colon_pos != std::string::npos) {
        newSensor.host = endpoint.substr(0, colon_pos);
        try {
          newSensor.port = std::stoi(endpoint.substr(colon_pos + 1));
        } catch (...) {
          newSensor.port = 10940;
        }
      } else {
        newSensor.host = endpoint;
        newSensor.port = 10940;
      }
    } else {
      newSensor.host = "192.168.1.10";
      newSensor.port = 10940;
    }
    
    // Validate port
    if (newSensor.port < 1 || newSensor.port > 65535) {
      Json::Value error;
      error["error"] = "invalid_port";
      error["message"] = "Port must be between 1 and 65535";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    // Parse other fields
    newSensor.mode = sensorData.get("mode", "ME").asString();
    if (newSensor.mode != "MD" && newSensor.mode != "ME") {
      Json::Value error;
      error["error"] = "invalid_mode";
      error["message"] = "Mode must be 'MD' or 'ME'";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    newSensor.interval = sensorData.get("interval", 0).asInt();
    newSensor.skip_step = std::max(1, sensorData.get("skip_step", 1).asInt());
    newSensor.ignore_checksum_error = sensorData.get("ignore_checksum_error", 1).asInt() ? 1 : 0;
    
    // Parse pose
    if (sensorData.isMember("pose") && sensorData["pose"].isObject()) {
      const auto& pose = sensorData["pose"];
      newSensor.pose.tx = pose.get("tx", 0.0f).asFloat();
      newSensor.pose.ty = pose.get("ty", 0.0f).asFloat();
      newSensor.pose.theta_deg = pose.get("theta_deg", 0.0f).asFloat();
    }
    
    // Parse mask
    if (sensorData.isMember("mask") && sensorData["mask"].isObject()) {
      const auto& mask = sensorData["mask"];
      if (mask.isMember("angle") && mask["angle"].isObject()) {
        const auto& angle = mask["angle"];
        newSensor.mask.angle.min_deg = angle.get("min_deg", -180.0f).asFloat();
        newSensor.mask.angle.max_deg = angle.get("max_deg", 180.0f).asFloat();
      }
      if (mask.isMember("range") && mask["range"].isObject()) {
        const auto& range = mask["range"];
        newSensor.mask.range.near_m = std::max(0.0f, range.get("near_m", 0.05f).asFloat());
        newSensor.mask.range.far_m = std::max(newSensor.mask.range.near_m, range.get("far_m", 15.0f).asFloat());
      }
    }
    
    // Add to configuration
    config_.sensors.push_back(newSensor);
    
    // Reload sensor manager
    sensors_.reloadFromAppConfig();
    
    // Notify all WebSocket clients
    if (ws_) {
      ws_->broadcastSnapshot();
    }
    
    // Return the created sensor
    Json::Value result;
    result["id"] = newSensor.id;
    result["type"] = newSensor.type;
    result["name"] = newSensor.name;
    result["enabled"] = newSensor.enabled;
    result["endpoint"] = newSensor.host + ":" + std::to_string(newSensor.port);
    result["mode"] = newSensor.mode;
    result["message"] = "Sensor added successfully";
    
    crow::response resp(201, result.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::deleteSensor(const std::string& id) {
  auto it = std::find_if(config_.sensors.begin(), config_.sensors.end(),
                        [&id](const SensorConfig& sensor) {
                          return sensor.id == id;
                        });
  
  if (it == config_.sensors.end()) {
    Json::Value error;
    error["error"] = "not_found";
    error["message"] = "Sensor not found";
    crow::response resp(404, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
  
  config_.sensors.erase(it);
  sensors_.reloadFromAppConfig();
  
  if (ws_) {
    ws_->broadcastSnapshot();
  }
  
  Json::Value result;
  result["id"] = id;
  result["message"] = "Sensor deleted successfully";
  
  crow::response resp(200, result.toStyledString());
  resp.add_header("Content-Type", "application/json");
  return resp;
}

// All remaining endpoints follow similar pattern...
// I'll implement the key ones to demonstrate the conversion

crow::response RestApi::getFilters() {
  try {
    Json::Value result = filters_.getFilterConfigAsJson();
    crow::response resp(200, result.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::getPrefilter() {
  try {
    Json::Value result = filters_.getPrefilterConfigAsJson();
    crow::response resp(200, result.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::putPrefilter(const crow::request& req) {
  if (!authorize(req)) {
    return sendUnauthorized();
  }
  
  try {
    Json::CharReaderBuilder builder;
    Json::Value config;
    std::string errs;
    std::istringstream stream(req.body);
    
    if (!Json::parseFromStream(builder, stream, &config, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    if (filters_.updatePrefilterConfig(config)) {
      Json::Value result = filters_.getPrefilterConfigAsJson();
      crow::response resp(200, result.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    } else {
      Json::Value error;
      error["error"] = "config_invalid";
      error["message"] = "Invalid prefilter configuration";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::getPostfilter() {
  try {
    Json::Value result = filters_.getPostfilterConfigAsJson();
    crow::response resp(200, result.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::putPostfilter(const crow::request& req) {
  if (!authorize(req)) {
    return sendUnauthorized();
  }
  
  try {
    Json::CharReaderBuilder builder;
    Json::Value config;
    std::string errs;
    std::istringstream stream(req.body);
    
    if (!Json::parseFromStream(builder, stream, &config, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    if (filters_.updatePostfilterConfig(config)) {
      Json::Value result = filters_.getPostfilterConfigAsJson();
      crow::response resp(200, result.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    } else {
      Json::Value error;
      error["error"] = "config_invalid";
      error["message"] = "Invalid postfilter configuration";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::getDbscan() {
  try {
    Json::Value result;
    result["eps_norm"] = config_.dbscan.eps_norm;
    result["minPts"] = config_.dbscan.minPts;
    result["k_scale"] = config_.dbscan.k_scale;
    result["h_min"] = config_.dbscan.h_min;
    result["h_max"] = config_.dbscan.h_max;
    result["R_max"] = config_.dbscan.R_max;
    result["M_max"] = config_.dbscan.M_max;
    
    crow::response resp(200, result.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::putDbscan(const crow::request& req) {
  if (!authorize(req)) {
    return sendUnauthorized();
  }
  
  try {
    Json::CharReaderBuilder builder;
    Json::Value config;
    std::string errs;
    std::istringstream stream(req.body);
    
    if (!Json::parseFromStream(builder, stream, &config, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    bool updated = false;
    
    // Validate and update eps_norm
    if (config.isMember("eps_norm")) {
      float eps_norm = config["eps_norm"].asFloat();
      if (eps_norm < 0.1f || eps_norm > 10.0f) {
        Json::Value error;
        error["error"] = "config_invalid";
        error["message"] = "eps_norm must be between 0.1 and 10.0";
        crow::response resp(400, error.toStyledString());
        resp.add_header("Content-Type", "application/json");
        return resp;
      }
      config_.dbscan.eps_norm = eps_norm;
      updated = true;
    }
    
    // Validate and update minPts
    if (config.isMember("minPts")) {
      int minPts = config["minPts"].asInt();
      if (minPts < 1 || minPts > 100) {
        Json::Value error;
        error["error"] = "config_invalid";
        error["message"] = "minPts must be between 1 and 100";
        crow::response resp(400, error.toStyledString());
        resp.add_header("Content-Type", "application/json");
        return resp;
      }
      config_.dbscan.minPts = minPts;
      updated = true;
    }
    
    // Validate and update k_scale
    if (config.isMember("k_scale")) {
      float k_scale = config["k_scale"].asFloat();
      if (k_scale < 0.1f || k_scale > 10.0f) {
        Json::Value error;
        error["error"] = "config_invalid";
        error["message"] = "k_scale must be between 0.1 and 10.0";
        crow::response resp(400, error.toStyledString());
        resp.add_header("Content-Type", "application/json");
        return resp;
      }
      config_.dbscan.k_scale = k_scale;
      updated = true;
    }
    
    // Validate and update h_min
    if (config.isMember("h_min")) {
      float h_min = config["h_min"].asFloat();
      if (h_min < 0.001f || h_min > config_.dbscan.h_max) {
        Json::Value error;
        error["error"] = "config_invalid";
        error["message"] = "h_min must be between 0.001 and h_max";
        crow::response resp(400, error.toStyledString());
        resp.add_header("Content-Type", "application/json");
        return resp;
      }
      config_.dbscan.h_min = h_min;
      updated = true;
    }
    
    // Validate and update h_max
    if (config.isMember("h_max")) {
      float h_max = config["h_max"].asFloat();
      if (h_max < config_.dbscan.h_min || h_max > 1.0f) {
        Json::Value error;
        error["error"] = "config_invalid";
        error["message"] = "h_max must be between h_min and 1.0";
        crow::response resp(400, error.toStyledString());
        resp.add_header("Content-Type", "application/json");
        return resp;
      }
      config_.dbscan.h_max = h_max;
      updated = true;
    }
    
    // Validate and update R_max
    if (config.isMember("R_max")) {
      int R_max = config["R_max"].asInt();
      if (R_max < 1 || R_max > 50) {
        Json::Value error;
        error["error"] = "config_invalid";
        error["message"] = "R_max must be between 1 and 50";
        crow::response resp(400, error.toStyledString());
        resp.add_header("Content-Type", "application/json");
        return resp;
      }
      config_.dbscan.R_max = R_max;
      updated = true;
    }
    
    // Validate and update M_max
    if (config.isMember("M_max")) {
      int M_max = config["M_max"].asInt();
      if (M_max < 10 || M_max > 5000) {
        Json::Value error;
        error["error"] = "config_invalid";
        error["message"] = "M_max must be between 10 and 5000";
        crow::response resp(400, error.toStyledString());
        resp.add_header("Content-Type", "application/json");
        return resp;
      }
      config_.dbscan.M_max = M_max;
      updated = true;
    }
    
    if (updated) {
      dbscan_.setParams(config_.dbscan.eps_norm, config_.dbscan.minPts);
      dbscan_.setAngularScale(config_.dbscan.k_scale);
      dbscan_.setPerformanceParams(config_.dbscan.h_min, config_.dbscan.h_max,
                                   config_.dbscan.R_max, config_.dbscan.M_max);
      
      if (ws_) {
        ws_->broadcastSnapshot();
      }
    }
    
    Json::Value result;
    result["eps_norm"] = config_.dbscan.eps_norm;
    result["minPts"] = config_.dbscan.minPts;
    result["k_scale"] = config_.dbscan.k_scale;
    result["h_min"] = config_.dbscan.h_min;
    result["h_max"] = config_.dbscan.h_max;
    result["R_max"] = config_.dbscan.R_max;
    result["M_max"] = config_.dbscan.M_max;
    
    crow::response resp(200, result.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

// Sinks endpoints
crow::response RestApi::getSinks() {
  try {
    Json::Value result(Json::arrayValue);
    
    for (size_t i = 0; i < config_.sinks.size(); ++i) {
      const auto& sink = config_.sinks[i];
      Json::Value sinkJson;
      
      sinkJson["index"] = static_cast<int>(i);
      sinkJson["topic"] = sink.topic;
      sinkJson["rate_limit"] = sink.rate_limit;
      
      if (sink.isOsc()) {
        sinkJson["type"] = "osc";
        sinkJson["url"] = sink.osc().url;
        sinkJson["in_bundle"] = sink.osc().in_bundle;
        sinkJson["bundle_fragment_size"] = static_cast<Json::UInt64>(sink.osc().bundle_fragment_size);
      } else if (sink.isNng()) {
        sinkJson["type"] = "nng";
        sinkJson["url"] = sink.nng().url;
        sinkJson["encoding"] = sink.nng().encoding;
      }
      
      result.append(sinkJson);
    }
    
    crow::response resp(200, result.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::postSink(const crow::request& req) {
  if (!authorize(req)) {
    return sendUnauthorized();
  }
  
  try {
    Json::CharReaderBuilder builder;
    Json::Value sinkData;
    std::string errs;
    std::istringstream stream(req.body);
    
    if (!Json::parseFromStream(builder, stream, &sinkData, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    // Validate required fields
    if (!sinkData.isMember("type") || !sinkData["type"].isString()) {
      Json::Value error;
      error["error"] = "missing_field";
      error["message"] = "Missing required field: type";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    if (!sinkData.isMember("url") || !sinkData["url"].isString()) {
      Json::Value error;
      error["error"] = "missing_field";
      error["message"] = "Missing required field: url";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    std::string type = sinkData["type"].asString();
    std::string url = sinkData["url"].asString();
    
    if (type != "nng" && type != "osc") {
      Json::Value error;
      error["error"] = "invalid_type";
      error["message"] = "Sink type must be 'nng' or 'osc'";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    // Validate URL format
    if (type == "nng" && url.find("tcp://") != 0) {
      Json::Value error;
      error["error"] = "invalid_url";
      error["message"] = "NNG sink URL must start with 'tcp://'";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    if (type == "osc" && url.find("osc://") != 0) {
      Json::Value error;
      error["error"] = "invalid_url";
      error["message"] = "OSC sink URL must start with 'osc://'";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    // Create new sink configuration
    SinkConfig newSink;
    newSink.topic = sinkData.get("topic", "clusters").asString();
    newSink.rate_limit = sinkData.get("rate_limit", 0).asInt();
    
    if (type == "osc") {
      OscConfig osc;
      osc.url = url;
      osc.in_bundle = sinkData.get("in_bundle", false).asBool();
      osc.bundle_fragment_size = sinkData.get("bundle_fragment_size", 0).asUInt64();
      newSink.cfg = osc;
    } else if (type == "nng") {
      NngConfig nng;
      nng.url = url;
      nng.encoding = sinkData.get("encoding", "msgpack").asString();
      
      if (nng.encoding != "msgpack" && nng.encoding != "json") {
        Json::Value error;
        error["error"] = "invalid_encoding";
        error["message"] = "NNG encoding must be 'msgpack' or 'json'";
        crow::response resp(400, error.toStyledString());
        resp.add_header("Content-Type", "application/json");
        return resp;
      }
      
      newSink.cfg = nng;
    }
    
    // Add to configuration
    config_.sinks.push_back(newSink);
    
    // Apply runtime configuration
    applySinksRuntime();
    
    // Notify WebSocket clients
    if (ws_) {
      ws_->broadcastSnapshot();
    }
    
    // Return created sink with index
    Json::Value result;
    result["index"] = static_cast<int>(config_.sinks.size() - 1);
    result["type"] = type;
    result["url"] = url;
    result["topic"] = newSink.topic;
    result["rate_limit"] = newSink.rate_limit;
    result["message"] = "Sink added successfully";
    
    crow::response resp(201, result.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::patchSink(int index, const crow::request& req) {
  if (!authorize(req)) {
    return sendUnauthorized();
  }
  
  try {
    if (index < 0 || index >= static_cast<int>(config_.sinks.size())) {
      Json::Value error;
      error["error"] = "not_found";
      error["message"] = "Sink not found";
      crow::response resp(404, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    Json::CharReaderBuilder builder;
    Json::Value patch;
    std::string errs;
    std::istringstream stream(req.body);
    
    if (!Json::parseFromStream(builder, stream, &patch, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    auto& sink = config_.sinks[index];
    bool updated = false;
    
    // Update basic fields
    if (patch.isMember("topic") && patch["topic"].isString()) {
      sink.topic = patch["topic"].asString();
      updated = true;
    }
    
    if (patch.isMember("rate_limit") && patch["rate_limit"].isInt()) {
      sink.rate_limit = patch["rate_limit"].asInt();
      updated = true;
    }
    
    // Update type-specific fields
    if (patch.isMember("url") && patch["url"].isString()) {
      std::string url = patch["url"].asString();
      
      if (sink.isOsc()) {
        if (url.find("osc://") != 0) {
          Json::Value error;
          error["error"] = "invalid_url";
          error["message"] = "OSC sink URL must start with 'osc://'";
          crow::response resp(400, error.toStyledString());
          resp.add_header("Content-Type", "application/json");
          return resp;
        }
        sink.osc().url = url;
        updated = true;
      } else if (sink.isNng()) {
        if (url.find("tcp://") != 0) {
          Json::Value error;
          error["error"] = "invalid_url";
          error["message"] = "NNG sink URL must start with 'tcp://'";
          crow::response resp(400, error.toStyledString());
          resp.add_header("Content-Type", "application/json");
          return resp;
        }
        sink.nng().url = url;
        updated = true;
      }
    }
    
    if (sink.isOsc()) {
      if (patch.isMember("in_bundle") && patch["in_bundle"].isBool()) {
        sink.osc().in_bundle = patch["in_bundle"].asBool();
        updated = true;
      }
      
      if (patch.isMember("bundle_fragment_size") && patch["bundle_fragment_size"].isUInt64()) {
        sink.osc().bundle_fragment_size = patch["bundle_fragment_size"].asUInt64();
        updated = true;
      }
    } else if (sink.isNng()) {
      if (patch.isMember("encoding") && patch["encoding"].isString()) {
        std::string encoding = patch["encoding"].asString();
        if (encoding != "msgpack" && encoding != "json") {
          Json::Value error;
          error["error"] = "invalid_encoding";
          error["message"] = "NNG encoding must be 'msgpack' or 'json'";
          crow::response resp(400, error.toStyledString());
          resp.add_header("Content-Type", "application/json");
          return resp;
        }
        sink.nng().encoding = encoding;
        updated = true;
      }
    }
    
    if (updated) {
      // Apply runtime configuration
      applySinksRuntime();
      
      // Notify WebSocket clients
      if (ws_) {
        ws_->broadcastSnapshot();
      }
    }
    
    // Return updated sink
    Json::Value result;
    result["index"] = index;
    result["topic"] = sink.topic;
    result["rate_limit"] = sink.rate_limit;
    
    if (sink.isOsc()) {
      result["type"] = "osc";
      result["url"] = sink.osc().url;
      result["in_bundle"] = sink.osc().in_bundle;
      result["bundle_fragment_size"] = static_cast<Json::UInt64>(sink.osc().bundle_fragment_size);
    } else if (sink.isNng()) {
      result["type"] = "nng";
      result["url"] = sink.nng().url;
      result["encoding"] = sink.nng().encoding;
    }
    
    result["message"] = "Sink updated successfully";
    
    crow::response resp(200, result.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::deleteSink(int index) {
  try {
    if (index < 0 || index >= static_cast<int>(config_.sinks.size())) {
      Json::Value error;
      error["error"] = "not_found";
      error["message"] = "Sink not found";
      crow::response resp(404, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    // Remove sink from configuration
    config_.sinks.erase(config_.sinks.begin() + index);
    
    // Apply runtime configuration
    applySinksRuntime();
    
    // Notify WebSocket clients
    if (ws_) {
      ws_->broadcastSnapshot();
    }
    
    Json::Value result;
    result["index"] = index;
    result["message"] = "Sink deleted successfully";
    
    crow::response resp(200, result.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::getSnapshot() {
  try {
    Json::Value snapshot;
    snapshot["sensors"] = sensors_.listAsJson();
    snapshot["filters"]["prefilter"] = filters_.getPrefilterConfigAsJson();
    snapshot["filters"]["postfilter"] = filters_.getPostfilterConfigAsJson();
    
    // DBSCAN config
    snapshot["dbscan"]["eps_norm"] = config_.dbscan.eps_norm;
    snapshot["dbscan"]["minPts"] = config_.dbscan.minPts;
    snapshot["dbscan"]["k_scale"] = config_.dbscan.k_scale;
    
    crow::response resp(200, snapshot.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::getConfigsList(const crow::request& req) {
  try {
    Json::Value result(Json::arrayValue);
    
    // List all .yaml files in the configs directory
    const std::string configsDir = "configs";
    if (!std::filesystem::exists(configsDir)) {
      Json::Value error;
      error["error"] = "configs_dir_not_found";
      error["message"] = "Configuration directory not found";
      crow::response resp(404, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(configsDir)) {
      if (entry.is_regular_file() && entry.path().extension() == ".yaml") {
        Json::Value configFile;
        configFile["name"] = entry.path().stem().string();
        configFile["filename"] = entry.path().filename().string();
        configFile["path"] = entry.path().string();
        
        // Get file size and modification time
        auto filesize = std::filesystem::file_size(entry.path());
        configFile["size"] = static_cast<Json::UInt64>(filesize);
        
        auto ftime = std::filesystem::last_write_time(entry.path());
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
          ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        auto time_t = std::chrono::system_clock::to_time_t(sctp);
        configFile["modified"] = static_cast<Json::Int64>(time_t);
        
        result.append(configFile);
      }
    }
    
    crow::response resp(200, result.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::postConfigsLoad(const crow::request& req) {
  if (!authorize(req)) {
    return sendUnauthorized();
  }
  
  try {
    Json::CharReaderBuilder builder;
    Json::Value requestData;
    std::string errs;
    std::istringstream stream(req.body);
    
    if (!Json::parseFromStream(builder, stream, &requestData, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }

    const std::string filename_name = "name";
    
    if (!requestData.isMember(filename_name) || !requestData[filename_name].isString()) {
      Json::Value error;
      error["error"] = "missing_field";
      error["message"] = "Missing required field: " + filename_name;
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }

    std::string filename = requestData[filename_name].asString();

    // Validate filename to prevent path traversal attacks
    if (filename.find("..") != std::string::npos ||
        filename.find("/") != std::string::npos ||
        filename.find("\\") != std::string::npos ||
        filename.empty()) {
      Json::Value error;
      error["error"] = "invalid_filename";
      error["message"] = "Invalid filename. Only simple filenames without path separators are allowed";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    // Ensure .yaml extension
    if (!filename.ends_with(".yaml")) {
      filename += ".yaml";
    }
    
    std::string configPath = "configs/" + filename;
    
    // Check if file exists
    if (!std::filesystem::exists(configPath)) {
      Json::Value error;
      error["error"] = "file_not_found";
      error["message"] = "Configuration file not found: " + filename;
      crow::response resp(404, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    // Load the configuration
    try {
      AppConfig newConfig = load_app_config(configPath);
      config_ = newConfig;
      
      // Apply the configuration to runtime systems
      sensors_.reloadFromAppConfig();
      filters_.recreatePrefilter();
      filters_.recreatePostfilter();
      dbscan_.setParams(config_.dbscan.eps_norm, config_.dbscan.minPts);
      dbscan_.setAngularScale(config_.dbscan.k_scale);
      dbscan_.setPerformanceParams(config_.dbscan.h_min, config_.dbscan.h_max,
                                   config_.dbscan.R_max, config_.dbscan.M_max);
      applySinksRuntime();
      
      // Notify WebSocket clients of configuration change
      if (ws_) {
        ws_->broadcastSnapshot();
      }
      
      Json::Value result;
      result["message"] = "Configuration loaded successfully";
      result["filename"] = filename;
      result["loaded_at"] = static_cast<Json::Int64>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
      
      crow::response resp(200, result.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
      
    } catch (const std::exception& configError) {
      Json::Value error;
      error["error"] = "config_load_failed";
      error["message"] = std::string("Failed to load configuration: ") + configError.what();
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::postConfigsImport(const crow::request& req) {
  if (!authorize(req)) {
    return sendUnauthorized();
  }
  
  try {
    Json::CharReaderBuilder builder;
    Json::Value requestData;
    std::string errs;
    std::istringstream stream(req.body);
    
    if (!Json::parseFromStream(builder, stream, &requestData, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    if (!requestData.isMember("yaml_content") || !requestData["yaml_content"].isString()) {
      Json::Value error;
      error["error"] = "missing_field";
      error["message"] = "Missing required field: yaml_content";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    std::string yamlContent = requestData["yaml_content"].asString();
    
    // Parse and validate the YAML content by attempting to load it
    try {
      // Create a temporary file to validate the YAML
      std::string tempPath = "/tmp/hokuyohub_config_temp.yaml";
      std::ofstream tempFile(tempPath);
      if (!tempFile.is_open()) {
        Json::Value error;
        error["error"] = "temp_file_error";
        error["message"] = "Failed to create temporary file for validation";
        crow::response resp(500, error.toStyledString());
        resp.add_header("Content-Type", "application/json");
        return resp;
      }
      
      tempFile << yamlContent;
      tempFile.close();
      
      // Load and validate the configuration
      AppConfig newConfig = load_app_config(tempPath);
      
      // Remove temporary file
      std::filesystem::remove(tempPath);
      
      // Apply the configuration
      config_ = newConfig;
      
      // Apply the configuration to runtime systems
      sensors_.reloadFromAppConfig();
      filters_.updatePrefilterConfig(Json::Value()); // Reload from config
      filters_.updatePostfilterConfig(Json::Value()); // Reload from config
      dbscan_.setParams(config_.dbscan.eps_norm, config_.dbscan.minPts);
      dbscan_.setAngularScale(config_.dbscan.k_scale);
      dbscan_.setPerformanceParams(config_.dbscan.h_min, config_.dbscan.h_max,
                                   config_.dbscan.R_max, config_.dbscan.M_max);
      applySinksRuntime();
      
      // Notify WebSocket clients of configuration change
      if (ws_) {
        ws_->broadcastSnapshot();
      }
      
      Json::Value result;
      result["message"] = "Configuration imported successfully";
      result["imported_at"] = static_cast<Json::Int64>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
      result["sensors_count"] = static_cast<int>(config_.sensors.size());
      result["sinks_count"] = static_cast<int>(config_.sinks.size());
      
      crow::response resp(200, result.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
      
    } catch (const std::exception& configError) {
      // Clean up temp file if it exists
      std::filesystem::remove("/tmp/hokuyohub_config_temp.yaml");
      
      Json::Value error;
      error["error"] = "invalid_config";
      error["message"] = std::string("Invalid YAML configuration: ") + configError.what();
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::postConfigsSave(const crow::request& req) {
  if (!authorize(req)) {
    return sendUnauthorized();
  }
  
  try {
    Json::CharReaderBuilder builder;
    Json::Value requestData;
    std::string errs;
    std::istringstream stream(req.body);
    
    if (!Json::parseFromStream(builder, stream, &requestData, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }

    const std::string filename_name = "name";
    
    if (!requestData.isMember(filename_name) || !requestData[filename_name].isString()) {
      Json::Value error;
      error["error"] = "missing_field";
      error["message"] = "Missing required field: " + filename_name;
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    std::string filename = requestData[filename_name].asString();
    
    // Validate filename to prevent path traversal attacks
    if (filename.find("..") != std::string::npos ||
        filename.find("/") != std::string::npos ||
        filename.find("\\") != std::string::npos ||
        filename.empty()) {
      Json::Value error;
      error["error"] = "invalid_filename";
      error["message"] = "Invalid filename. Only simple filenames without path separators are allowed";
      crow::response resp(400, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    // Ensure .yaml extension
    if (!filename.ends_with(".yaml")) {
      filename += ".yaml";
    }
    
    std::string configPath = "configs/" + filename;
    
    // Create configs directory if it doesn't exist
    std::filesystem::create_directories("configs");
    
    // Generate the YAML content from current configuration
    std::string yamlContent = dump_app_config(config_);
    
    // Save the configuration to file
    std::ofstream configFile(configPath);
    if (!configFile.is_open()) {
      Json::Value error;
      error["error"] = "file_write_error";
      error["message"] = "Failed to open configuration file for writing: " + filename;
      crow::response resp(500, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    configFile << yamlContent;
    configFile.close();
    
    if (configFile.fail()) {
      Json::Value error;
      error["error"] = "file_write_error";
      error["message"] = "Failed to write configuration file: " + filename;
      crow::response resp(500, error.toStyledString());
      resp.add_header("Content-Type", "application/json");
      return resp;
    }
    
    Json::Value result;
    result["message"] = "Configuration saved successfully";
    result[filename_name] = filename;
    result["path"] = configPath;
    result["saved_at"] = static_cast<Json::Int64>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    result["size"] = static_cast<Json::UInt64>(yamlContent.length());
    
    crow::response resp(200, result.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
    
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

crow::response RestApi::getConfigsExport() {
  try {
    // Generate the YAML content from current configuration
    std::string yamlContent = dump_app_config(config_);
    
    // Return the YAML content directly with appropriate content type
    crow::response resp(200, yamlContent);
    resp.add_header("Content-Type", "application/x-yaml");
    resp.add_header("Content-Disposition", "attachment; filename=\"exported_config.yaml\"");
    return resp;
    
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    crow::response resp(500, error.toStyledString());
    resp.add_header("Content-Type", "application/json");
    return resp;
  }
}

void RestApi::applySinksRuntime() {
  std::cout << "[RestApi] Applying sink configuration to runtime..." << std::endl;
  
  bool success = publisher_manager_.configure(config_.sinks);
  
  if (success) {
    std::cout << "[RestApi] All sinks configured successfully" << std::endl;
  } else {
    std::cout << "[RestApi] Some sinks failed to configure (see logs above)" << std::endl;
  }
  
  std::cout << "[RestApi] Sink runtime configuration complete - "
            << publisher_manager_.getEnabledPublisherCount() << " of "
            << publisher_manager_.getPublisherCount() << " publishers active" << std::endl;
}