#include "rest_handlers.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <regex>

bool RestApi::authorize(const drogon::HttpRequestPtr& req) const {
  if (token_.empty()) return true; // Auth disabled if no token
  
  auto auth = req->getHeader("Authorization");
  if (auth.empty()) return false;
  
  const std::string bearer_prefix = "Bearer ";
  if (auth.size() <= bearer_prefix.size() || 
      auth.substr(0, bearer_prefix.size()) != bearer_prefix) {
    return false;
  }
  
  return auth.substr(bearer_prefix.size()) == token_;
}

void RestApi::sendUnauthorized(std::function<void (const drogon::HttpResponsePtr &)>&& callback) const {
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(drogon::k401Unauthorized);
  resp->addHeader("WWW-Authenticate", "Bearer realm=\"api\", error=\"invalid_token\"");
  Json::Value error;
  error["error"] = "unauthorized";
  error["message"] = "Invalid or missing authorization token";
  resp->setBody(error.toStyledString());
  resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  callback(resp);
}

// Sensors endpoints
void RestApi::getSensors(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  try {
    Json::Value sensors = sensors_.listAsJson();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(sensors);
    callback(resp);
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

void RestApi::getSensorById(const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  try {
    // REST API uses slot index (numeric) for sensor operations
    std::string sensor_id = req->getRoutingParameters()[0];
    Json::Value sensor = sensors_.getAsJson(sensor_id);

    if (sensor.isNull() || !sensor.isMember("id")) {
      Json::Value error;
      error["error"] = "not_found";
      error["message"] = "Sensor not found";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k404NotFound);
      callback(resp);
      return;
    }
    
    auto resp = drogon::HttpResponse::newHttpJsonResponse(sensor);
    callback(resp);
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "invalid_id";
    error["message"] = "Invalid sensor slot index";
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
  }
}

void RestApi::patchSensor(const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  if (!authorize(req)) {
    sendUnauthorized(std::move(callback));
    return;
  }
  
  try {
    // REST API uses slot index (numeric) for sensor operations
    std::string sensor_id = req->getRoutingParameters()[0];
    
    Json::CharReaderBuilder builder;
    Json::Value patch;
    std::string errs;
    std::istringstream stream(std::string(req->getBody()));
    
    if (!Json::parseFromStream(builder, stream, &patch, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    Json::Value applied;
    std::string err;

    if (sensors_.applyPatch(sensor_id, patch, applied, err)) {
      Json::Value result;
      result["id"] = sensor_id;
      result["applied"] = applied;
      result["sensor"] = sensors_.getAsJson(sensor_id);
      auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
      callback(resp);
    } else {
      Json::Value error;
      error["error"] = "patch_failed";
      error["message"] = err;
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
    }
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

// Sensor addition endpoint
void RestApi::postSensor(const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  if (!authorize(req)) {
    sendUnauthorized(std::move(callback));
    return;
  }
  
  try {
    Json::CharReaderBuilder builder;
    Json::Value sensorData;
    std::string errs;
    std::istringstream stream(std::string(req->getBody()));
    
    if (!Json::parseFromStream(builder, stream, &sensorData, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    // Validate required fields
    if (!sensorData.isMember("type") || !sensorData["type"].isString()) {
      Json::Value error;
      error["error"] = "missing_field";
      error["message"] = "Missing required field: type";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    std::string type = sensorData["type"].asString();
    if (type != "hokuyo_urg_eth" && type != "unknown") {
      Json::Value error;
      error["error"] = "invalid_type";
      error["message"] = "Sensor type must be 'hokuyo_urg_eth' or 'unknown'";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    // Generate unique sensor ID
    std::string newId = sensorData.isMember("name") ? sensorData["name"].asString() : "sensor";
    int appendix_max = 0;
    // find existing sensor with same name or same name ends with numbers
    for (const auto& sensor : config_.sensors) {
      if (sensor.id.starts_with(newId)) {
        // check if after 1 white space it ends with a number
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
          newSensor.port = 10940; // default
        }
      } else {
        newSensor.host = endpoint;
        newSensor.port = 10940; // default
      }
    } else {
      newSensor.host = "192.168.1.10"; // default
      newSensor.port = 10940; // default
    }
    
    // Validate port
    if (newSensor.port < 1 || newSensor.port > 65535) {
      Json::Value error;
      error["error"] = "invalid_port";
      error["message"] = "Port must be between 1 and 65535";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    // Parse other fields
    newSensor.mode = sensorData.get("mode", "ME").asString();
    if (newSensor.mode != "MD" && newSensor.mode != "ME") {
      Json::Value error;
      error["error"] = "invalid_mode";
      error["message"] = "Mode must be 'MD' or 'ME'";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
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
    
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    resp->setStatusCode(drogon::k201Created);
    callback(resp);
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

// Sensor deletion endpoint
void RestApi::deleteSensor(const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  if (!authorize(req)) {
    sendUnauthorized(std::move(callback));
    return;
  }
  
  try {
    // This endpoint uses string sensor ID (cfg.id) for deletion
    std::string sensor_id = req->getRoutingParameters()[0];

    // Find sensor in configuration by string ID
    auto it = std::find_if(config_.sensors.begin(), config_.sensors.end(),
                          [&sensor_id](const SensorConfig& sensor) {
                            return sensor.id == sensor_id;
                          });
    
    if (it == config_.sensors.end()) {
      Json::Value error;
      error["error"] = "not_found";
      error["message"] = "Sensor not found";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k404NotFound);
      callback(resp);
      return;
    }
    
    // Remove sensor from configuration
    config_.sensors.erase(it);
    
    // Reload sensor manager
    sensors_.reloadFromAppConfig();
    
    // Notify all WebSocket clients
    if (ws_) {
      ws_->broadcastSnapshot();
    }
    
    Json::Value result;
    result["id"] = sensor_id;
    result["message"] = "Sensor deleted successfully";
    
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    callback(resp);
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

// Filter endpoints
void RestApi::getFilters(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  try {
    Json::Value result = filters_.getFilterConfigAsJson();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    callback(resp);
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

void RestApi::getPrefilter(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  try {
    Json::Value result = filters_.getPrefilterConfigAsJson();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    callback(resp);
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

void RestApi::putPrefilter(const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  if (!authorize(req)) {
    sendUnauthorized(std::move(callback));
    return;
  }
  
  try {
    Json::CharReaderBuilder builder;
    Json::Value config;
    std::string errs;
    std::istringstream stream(std::string(req->getBody()));
    
    if (!Json::parseFromStream(builder, stream, &config, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    if (filters_.updatePrefilterConfig(config)) {
      Json::Value result = filters_.getPrefilterConfigAsJson();
      auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
      callback(resp);
    } else {
      Json::Value error;
      error["error"] = "config_invalid";
      error["message"] = "Invalid prefilter configuration";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
    }
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

void RestApi::getPostfilter(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  try {
    Json::Value result = filters_.getPostfilterConfigAsJson();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    callback(resp);
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

void RestApi::putPostfilter(const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  if (!authorize(req)) {
    sendUnauthorized(std::move(callback));
    return;
  }
  
  try {
    Json::CharReaderBuilder builder;
    Json::Value config;
    std::string errs;
    std::istringstream stream(std::string(req->getBody()));
    
    if (!Json::parseFromStream(builder, stream, &config, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    if (filters_.updatePostfilterConfig(config)) {
      Json::Value result = filters_.getPostfilterConfigAsJson();
      auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
      callback(resp);
    } else {
      Json::Value error;
      error["error"] = "config_invalid";
      error["message"] = "Invalid postfilter configuration";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
    }
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

// DBSCAN endpoints
void RestApi::getDbscan(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  try {
    Json::Value result;
    result["eps_norm"] = config_.dbscan.eps_norm;
    result["minPts"] = config_.dbscan.minPts;
    result["k_scale"] = config_.dbscan.k_scale;
    result["h_min"] = config_.dbscan.h_min;
    result["h_max"] = config_.dbscan.h_max;
    result["R_max"] = config_.dbscan.R_max;
    result["M_max"] = config_.dbscan.M_max;
    
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    callback(resp);
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

void RestApi::putDbscan(const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  if (!authorize(req)) {
    sendUnauthorized(std::move(callback));
    return;
  }
  
  try {
    Json::CharReaderBuilder builder;
    Json::Value config;
    std::string errs;
    std::istringstream stream(std::string(req->getBody()));
    
    if (!Json::parseFromStream(builder, stream, &config, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    // Validate and update DBSCAN configuration
    bool updated = false;
    
    if (config.isMember("eps_norm")) {
      float eps_norm = config["eps_norm"].asFloat();
      if (eps_norm < 0.1f || eps_norm > 10.0f) {
        Json::Value error;
        error["error"] = "config_invalid";
        error["message"] = "eps_norm must be between 0.1 and 10.0";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
      }
      config_.dbscan.eps_norm = eps_norm;
      updated = true;
    }
    
    if (config.isMember("minPts")) {
      int minPts = config["minPts"].asInt();
      if (minPts < 1 || minPts > 100) {
        Json::Value error;
        error["error"] = "config_invalid";
        error["message"] = "minPts must be between 1 and 100";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
      }
      config_.dbscan.minPts = minPts;
      updated = true;
    }
    
    if (config.isMember("k_scale")) {
      float k_scale = config["k_scale"].asFloat();
      if (k_scale < 0.1f || k_scale > 10.0f) {
        Json::Value error;
        error["error"] = "config_invalid";
        error["message"] = "k_scale must be between 0.1 and 10.0";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
      }
      config_.dbscan.k_scale = k_scale;
      updated = true;
    }
    
    if (config.isMember("h_min")) {
      float h_min = config["h_min"].asFloat();
      if (h_min < 0.001f || h_min > config_.dbscan.h_max) {
        Json::Value error;
        error["error"] = "config_invalid";
        error["message"] = "h_min must be between 0.001 and h_max";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
      }
      config_.dbscan.h_min = h_min;
      updated = true;
    }
    
    if (config.isMember("h_max")) {
      float h_max = config["h_max"].asFloat();
      if (h_max < config_.dbscan.h_min || h_max > 1.0f) {
        Json::Value error;
        error["error"] = "config_invalid";
        error["message"] = "h_max must be between h_min and 1.0";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
      }
      config_.dbscan.h_max = h_max;
      updated = true;
    }
    
    if (config.isMember("R_max")) {
      int R_max = config["R_max"].asInt();
      if (R_max < 1 || R_max > 50) {
        Json::Value error;
        error["error"] = "config_invalid";
        error["message"] = "R_max must be between 1 and 50";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
      }
      config_.dbscan.R_max = R_max;
      updated = true;
    }
    
    if (config.isMember("M_max")) {
      int M_max = config["M_max"].asInt();
      if (M_max < 10 || M_max > 5000) {
        Json::Value error;
        error["error"] = "config_invalid";
        error["message"] = "M_max must be between 10 and 5000";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
      }
      config_.dbscan.M_max = M_max;
      updated = true;
    }
    
    if (updated) {
      // Apply the updated configuration to the DBSCAN instance
      dbscan_.setParams(config_.dbscan.eps_norm, config_.dbscan.minPts);
      dbscan_.setAngularScale(config_.dbscan.k_scale);
      dbscan_.setPerformanceParams(config_.dbscan.h_min, config_.dbscan.h_max,
                                   config_.dbscan.R_max, config_.dbscan.M_max);
      
      std::cout << "[DBSCAN] Configuration updated via REST API: eps_norm=" << config_.dbscan.eps_norm
                << " minPts=" << config_.dbscan.minPts << " k_scale=" << config_.dbscan.k_scale << std::endl;
      
      // Notify all WebSocket clients about the configuration change
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
    
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    callback(resp);
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

// Sink endpoints
void RestApi::getSinks(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  try {
    Json::Value sinks = Json::arrayValue;
    
    for (size_t i = 0; i < config_.sinks.size(); ++i) {
      const auto& sink = config_.sinks[i];
      Json::Value sink_obj;
      sink_obj["index"] = static_cast<int>(i);
      sink_obj["enabled"] = true; // Assume enabled if in config
      sink_obj["topic"] = sink.topic;
      sink_obj["rate_limit"] = sink.rate_limit;
      
      if (sink.isOsc()) {
        const auto& cfg = sink.osc();
        sink_obj["type"] = "osc";
        sink_obj["url"] = cfg.url;
        sink_obj["in_bundle"] = cfg.in_bundle;
        sink_obj["bundle_fragment_size"] = static_cast<int>(cfg.bundle_fragment_size);
      } else if (sink.isNng()) {
        const auto& cfg = sink.nng();
        sink_obj["type"] = "nng";
        sink_obj["url"] = cfg.url;
        sink_obj["encoding"] = cfg.encoding;
      }
      
      sinks.append(sink_obj);
    }
    
    auto resp = drogon::HttpResponse::newHttpJsonResponse(sinks);
    callback(resp);
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

void RestApi::postSink(const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  if (!authorize(req)) {
    sendUnauthorized(std::move(callback));
    return;
  }
  
  try {
    Json::CharReaderBuilder builder;
    Json::Value sinkData;
    std::string errs;
    std::istringstream stream(std::string(req->getBody()));
    
    if (!Json::parseFromStream(builder, stream, &sinkData, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    // Validate required fields
    if (!sinkData.isMember("type") || !sinkData["type"].isString()) {
      Json::Value error;
      error["error"] = "missing_field";
      error["message"] = "Missing required field: type";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    std::string type = sinkData["type"].asString();
    if (type != "nng" && type != "osc") {
      Json::Value error;
      error["error"] = "invalid_type";
      error["message"] = "Sink type must be 'nng' or 'osc'";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    if (!sinkData.isMember("url") || !sinkData["url"].isString()) {
      Json::Value error;
      error["error"] = "missing_field";
      error["message"] = "Missing required field: url";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    SinkConfig newSink;
    newSink.topic = sinkData.get("topic", "clusters").asString();
    newSink.rate_limit = std::max(0, sinkData.get("rate_limit", 0).asInt());
    
    std::string url = sinkData["url"].asString();
    
    if (type == "nng") {
      NngConfig nngCfg;
      nngCfg.url = url;
      nngCfg.encoding = sinkData.get("encoding", "msgpack").asString();
      
      // Validate encoding
      if (nngCfg.encoding != "msgpack" && nngCfg.encoding != "json") {
        Json::Value error;
        error["error"] = "invalid_encoding";
        error["message"] = "Encoding must be 'msgpack' or 'json'";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
      }
      
      newSink.cfg = nngCfg;
    } else { // osc
      OscConfig oscCfg;
      oscCfg.url = url;
      oscCfg.in_bundle = sinkData.get("in_bundle", false).asBool();
      oscCfg.bundle_fragment_size = std::max(0ULL, static_cast<uint64_t>(sinkData.get("bundle_fragment_size", 0).asInt()));
      
      newSink.cfg = oscCfg;
    }
    
    // Add to configuration
    config_.sinks.push_back(newSink);
    
    // Apply sink configuration to runtime
    applySinksRuntime();
    
    // Notify all WebSocket clients
    if (ws_) {
      ws_->broadcastSnapshot();
    }
    
    // Return the created sink
    Json::Value result;
    result["index"] = static_cast<int>(config_.sinks.size() - 1);
    result["type"] = type;
    result["url"] = url;
    result["topic"] = newSink.topic;
    result["rate_limit"] = newSink.rate_limit;
    result["message"] = "Sink added successfully";
    
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    resp->setStatusCode(drogon::k201Created);
    callback(resp);
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

void RestApi::patchSink(const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  if (!authorize(req)) {
    sendUnauthorized(std::move(callback));
    return;
  }
  
  try {
    for (const auto& param : req->getRoutingParameters()) {
      std::cout << "Routing parameter: " << param << std::endl;
    }
    int index = std::stoi(req->getRoutingParameters()[0]);
    std::cout << "Patching sink: " << index << std::endl;
    if (index < 0 || index >= static_cast<int>(config_.sinks.size())) {
      Json::Value error;
      error["error"] = "invalid_index";
      error["message"] = "Sink index out of range";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k404NotFound);
      callback(resp);
      return;
    }
    
    Json::CharReaderBuilder builder;
    Json::Value patch;
    std::string errs;
    std::istringstream stream(std::string(req->getBody()));
    
    if (!Json::parseFromStream(builder, stream, &patch, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    auto& sink = config_.sinks[index];
    
    // Don't allow type changes
    if (patch.isMember("type")) {
      Json::Value error;
      error["error"] = "type_immutable";
      error["message"] = "Sink type cannot be changed";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    // Update common fields
    if (patch.isMember("topic")) {
      sink.topic = patch["topic"].asString();
    }
    
    if (patch.isMember("rate_limit")) {
      sink.rate_limit = std::max(0, patch["rate_limit"].asInt());
    }
    
    // Update type-specific fields
    if (patch.isMember("url")) {
      std::string url = patch["url"].asString();
      if (sink.isNng()) {
        sink.nng().url = url;
      } else if (sink.isOsc()) {
        sink.osc().url = url;
      }
    }
    
    if (sink.isNng()) {
      if (patch.isMember("encoding")) {
        std::string encoding = patch["encoding"].asString();
        if (encoding != "msgpack" && encoding != "json") {
          Json::Value error;
          error["error"] = "invalid_encoding";
          error["message"] = "Encoding must be 'msgpack' or 'json'";
          auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
          resp->setStatusCode(drogon::k400BadRequest);
          callback(resp);
          return;
        }
        sink.nng().encoding = encoding;
      }
    }
    
    if (sink.isOsc()) {
      if (patch.isMember("in_bundle")) {
        sink.osc().in_bundle = patch["in_bundle"].asBool();
      }
      if (patch.isMember("bundle_fragment_size")) {
        sink.osc().bundle_fragment_size = std::max(0ULL, static_cast<uint64_t>(patch["bundle_fragment_size"].asInt()));
      }
    }
    
    // Apply sink configuration to runtime
    applySinksRuntime();
    
    // Notify all WebSocket clients
    if (ws_) {
      ws_->broadcastSnapshot();
    }
    
    // Return updated sink
    Json::Value result;
    result["index"] = index;
    result["topic"] = sink.topic;
    result["rate_limit"] = sink.rate_limit;
    result["message"] = "Sink updated successfully";
    
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    callback(resp);
  } catch (const std::invalid_argument&) {
    Json::Value error;
    error["error"] = "invalid_index";
    error["message"] = "Invalid sink index";
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

void RestApi::deleteSink(const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  if (!authorize(req)) {
    sendUnauthorized(std::move(callback));
    return;
  }
  
  try {
    int index = std::stoi(req->getRoutingParameters()[0]);
    
    if (index < 0 || index >= static_cast<int>(config_.sinks.size())) {
      Json::Value error;
      error["error"] = "invalid_index";
      error["message"] = "Sink index out of range";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k404NotFound);
      callback(resp);
      return;
    }
    
    // Remove sink from configuration
    config_.sinks.erase(config_.sinks.begin() + index);
    
    // Apply sink configuration to runtime
    applySinksRuntime();
    
    // Notify all WebSocket clients
    if (ws_) {
      ws_->broadcastSnapshot();
    }
    
    Json::Value result;
    result["message"] = "Sink deleted successfully";
    
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    callback(resp);
  } catch (const std::invalid_argument&) {
    Json::Value error;
    error["error"] = "invalid_index";
    error["message"] = "Invalid sink index";
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

// Snapshot endpoint
void RestApi::getSnapshot(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  try {
    Json::Value snapshot;
    
    // Sensors
    snapshot["sensors"] = sensors_.listAsJson();
    
    // Filters
    snapshot["filters"]["prefilter"] = filters_.getPrefilterConfigAsJson();
    snapshot["filters"]["postfilter"] = filters_.getPostfilterConfigAsJson();
    
    // DBSCAN config
    snapshot["dbscan"]["eps_norm"] = config_.dbscan.eps_norm;
    snapshot["dbscan"]["minPts"] = config_.dbscan.minPts;
    snapshot["dbscan"]["k_scale"] = config_.dbscan.k_scale;
    snapshot["dbscan"]["h_min"] = config_.dbscan.h_min;
    snapshot["dbscan"]["h_max"] = config_.dbscan.h_max;
    snapshot["dbscan"]["R_max"] = config_.dbscan.R_max;
    snapshot["dbscan"]["M_max"] = config_.dbscan.M_max;
    
    // World mask
    snapshot["world_mask"]["include_count"] = static_cast<int>(config_.world_mask.include.size());
    snapshot["world_mask"]["exclude_count"] = static_cast<int>(config_.world_mask.exclude.size());
    
    // Sinks/Publishers
    Json::Value publishers;
    Json::Value sinks_array = Json::arrayValue;
    
    // Build new sinks array format
    for (const auto& sink : config_.sinks) {
      Json::Value sink_obj;
      sink_obj["enabled"] = true; // Assume enabled if in config
      if(sink.isOsc()) {
        const auto& cfg = sink.osc();
        sink_obj["type"] = "osc";
        sink_obj["url"] = cfg.url;
        sink_obj["in_bundle"] = cfg.in_bundle;
        sink_obj["bundle_fragment_size"] = cfg.bundle_fragment_size;
      }
      if(sink.isNng()) {
        const auto& cfg = sink.nng();
        sink_obj["type"] = "nng";
        sink_obj["url"] = cfg.url;
        sink_obj["encoding"] = cfg.encoding;
      }
      if (!sink.topic.empty()) {
        sink_obj["topic"] = sink.topic;
      }
      sink_obj["rate_limit"] = sink.rate_limit;
      
      sinks_array.append(sink_obj);
    }
        
    // Add both formats
    publishers["sinks"] = sinks_array;
    snapshot["publishers"] = publishers;
    
    // UI config
    snapshot["ui"]["listen"] = config_.ui.listen;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(snapshot);
    callback(resp);
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

// Config endpoints
void RestApi::getConfigsList(const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  if (!authorize(req)) {
    sendUnauthorized(std::move(callback));
    return;
  }
  
  try {
    Json::Value result;
    result["files"] = Json::arrayValue;
    
    const std::string configs_dir = "config";
    
    if (std::filesystem::exists(configs_dir) && std::filesystem::is_directory(configs_dir)) {
      for (const auto& entry : std::filesystem::directory_iterator(configs_dir)) {
        if (entry.is_regular_file()) {
          std::string filename = entry.path().filename().string();
          if (filename.ends_with(".yaml") || filename.ends_with(".yml")) {
            // Remove extension
            size_t dot_pos = filename.find_last_of('.');
            if (dot_pos != std::string::npos) {
              std::string name = filename.substr(0, dot_pos);
              result["files"].append(name);
            }
          }
        }
      }
    }
    
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    callback(resp);
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

void RestApi::postConfigsLoad(const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  if (!authorize(req)) {
    sendUnauthorized(std::move(callback));
    return;
  }
  
  try {
    Json::CharReaderBuilder builder;
    Json::Value body;
    std::string errs;
    std::istringstream stream(std::string(req->getBody()));
    
    if (!Json::parseFromStream(builder, stream, &body, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    // Get name from request body
    if (!body.isMember("name") || !body["name"].isString()) {
      Json::Value error;
      error["error"] = "missing_name";
      error["message"] = "Missing required 'name' field in request body";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    std::string name = body["name"].asString();
    
    // Validate name (only alphanumeric, underscore, hyphen)
    std::regex name_regex("^[A-Za-z0-9_-]+$");
    if (!std::regex_match(name, name_regex)) {
      Json::Value error;
      error["error"] = "invalid_name";
      error["message"] = "Name can only contain letters, numbers, underscores, and hyphens";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    std::string path = "config/" + name + ".yaml";
    
    try {
      AppConfig new_config = load_app_config(path);
      config_ = new_config;

      // Reload configurations in SensorManager and FilterManager
      sensors_.reloadFromAppConfig();
      filters_.reloadFromAppConfig();
      
      // Apply DBSCAN configuration to runtime
      dbscan_.setParams(config_.dbscan.eps_norm, config_.dbscan.minPts);
      dbscan_.setAngularScale(config_.dbscan.k_scale);
      dbscan_.setPerformanceParams(config_.dbscan.h_min, config_.dbscan.h_max,
                                   config_.dbscan.R_max, config_.dbscan.M_max);
      
      std::cout << "[DBSCAN] Configuration loaded from file: eps_norm=" << config_.dbscan.eps_norm
                << " minPts=" << config_.dbscan.minPts << " k_scale=" << config_.dbscan.k_scale << std::endl;
      
      // Apply sink configuration to runtime
      applySinksRuntime();
      
      // Notify all WebSocket clients about the configuration change
      if (ws_) {
        // Broadcast DBSCAN config update specifically
        Json::Value broadcast_msg;
        broadcast_msg["type"] = "dbscan.updated";
        broadcast_msg["config"]["eps_norm"] = config_.dbscan.eps_norm;
        broadcast_msg["config"]["minPts"] = config_.dbscan.minPts;
        broadcast_msg["config"]["k_scale"] = config_.dbscan.k_scale;
        broadcast_msg["config"]["h_min"] = config_.dbscan.h_min;
        broadcast_msg["config"]["h_max"] = config_.dbscan.h_max;
        broadcast_msg["config"]["R_max"] = config_.dbscan.R_max;
        broadcast_msg["config"]["M_max"] = config_.dbscan.M_max;
        
        ws_->broadcast(broadcast_msg.toStyledString());
        
        // Also broadcast full snapshot
        ws_->broadcastSnapshot();
      }
      
      Json::Value result;
      result["loaded"] = true;
      result["name"] = name;
      result["message"] = "Configuration loaded successfully";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
      callback(resp);
    } catch (const std::exception& e) {
      Json::Value error;
      error["error"] = "load_failed";
      error["message"] = std::string("Failed to load config: ") + e.what();
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
    }
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

void RestApi::postConfigsImport(const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  if (!authorize(req)) {
    sendUnauthorized(std::move(callback));
    return;
  }
  
  try {
    std::string yaml_content = std::string(req->getBody());
    
    // Write to temporary file and load
    std::string temp_path = "/tmp/hokuyo_import_" + std::to_string(time(nullptr)) + ".yaml";
    std::ofstream temp_file(temp_path);
    temp_file << yaml_content;
    temp_file.close();
    
    try {
      AppConfig new_config = load_app_config(temp_path);
      config_ = new_config;
      
      // Reload configurations in SensorManager and FilterManager
      sensors_.reloadFromAppConfig();
      filters_.reloadFromAppConfig();
      
      // Apply DBSCAN configuration to runtime
      dbscan_.setParams(config_.dbscan.eps_norm, config_.dbscan.minPts);
      dbscan_.setAngularScale(config_.dbscan.k_scale);
      dbscan_.setPerformanceParams(config_.dbscan.h_min, config_.dbscan.h_max,
                                   config_.dbscan.R_max, config_.dbscan.M_max);
      
      std::cout << "[DBSCAN] Configuration imported: eps_norm=" << config_.dbscan.eps_norm
                << " minPts=" << config_.dbscan.minPts << " k_scale=" << config_.dbscan.k_scale << std::endl;
      
      // Apply sink configuration to runtime
      applySinksRuntime();
      
      // Notify all WebSocket clients about the configuration change
      if (ws_) {
        // Broadcast DBSCAN config update specifically
        Json::Value broadcast_msg;
        broadcast_msg["type"] = "dbscan.updated";
        broadcast_msg["config"]["eps_norm"] = config_.dbscan.eps_norm;
        broadcast_msg["config"]["minPts"] = config_.dbscan.minPts;
        broadcast_msg["config"]["k_scale"] = config_.dbscan.k_scale;
        broadcast_msg["config"]["h_min"] = config_.dbscan.h_min;
        broadcast_msg["config"]["h_max"] = config_.dbscan.h_max;
        broadcast_msg["config"]["R_max"] = config_.dbscan.R_max;
        broadcast_msg["config"]["M_max"] = config_.dbscan.M_max;
        
        ws_->broadcast(broadcast_msg.toStyledString());
        
        // Also broadcast full snapshot
        ws_->broadcastSnapshot();
      }
      
      // Clean up temp file
      std::remove(temp_path.c_str());
      
      Json::Value result;
      result["imported"] = true;
      result["message"] = "Configuration imported successfully";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
      callback(resp);
    } catch (const std::exception& e) {
      std::remove(temp_path.c_str());
      Json::Value error;
      error["error"] = "import_failed";
      error["message"] = std::string("Failed to import config: ") + e.what();
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
    }
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

void RestApi::postConfigsSave(const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  if (!authorize(req)) {
    sendUnauthorized(std::move(callback));
    return;
  }
  
  try {
    Json::CharReaderBuilder builder;
    Json::Value body;
    std::string errs;
    std::istringstream stream(std::string(req->getBody()));
    
    if (!Json::parseFromStream(builder, stream, &body, &errs)) {
      Json::Value error;
      error["error"] = "invalid_json";
      error["message"] = "Invalid JSON in request body";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    // Get name from request body
    if (!body.isMember("name") || !body["name"].isString()) {
      Json::Value error;
      error["error"] = "missing_name";
      error["message"] = "Missing required 'name' field in request body";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    std::string name = body["name"].asString();
    
    // Validate name (only alphanumeric, underscore, hyphen)
    std::regex name_regex("^[A-Za-z0-9_-]+$");
    if (!std::regex_match(name, name_regex)) {
      Json::Value error;
      error["error"] = "invalid_name";
      error["message"] = "Name can only contain letters, numbers, underscores, and hyphens";
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k400BadRequest);
      callback(resp);
      return;
    }
    
    std::string path = "config/" + name + ".yaml";
    
    // Ensure config directory exists
    std::filesystem::create_directories("config");
    
    std::string yaml_content = dump_app_config(config_);
    
    std::ofstream file(path);
    if (!file.is_open()) {
      Json::Value error;
      error["error"] = "save_failed";
      error["message"] = "Could not open file for writing: " + path;
      auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(drogon::k500InternalServerError);
      callback(resp);
      return;
    }
    
    file << yaml_content;
    file.close();
    
    Json::Value result;
    result["saved"] = true;
    result["name"] = name;
    result["bytes"] = static_cast<int>(yaml_content.size());
    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    callback(resp);
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

void RestApi::getConfigsExport(const drogon::HttpRequestPtr&, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
  try {
    std::string yaml_content = dump_app_config(config_);
    
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setBody(yaml_content);
    resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
    resp->addHeader("Content-Disposition", "attachment; filename=\"hokuyo_config.yaml\"");
    callback(resp);
  } catch (const std::exception& e) {
    Json::Value error;
    error["error"] = "internal_error";
    error["message"] = e.what();
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(drogon::k500InternalServerError);
    callback(resp);
  }
}

// Sink runtime management
void RestApi::applySinksRuntime() {
  std::cout << "[RestApi] Applying sink configuration to runtime..." << std::endl;
  
  // Configure all sinks through PublisherManager
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
