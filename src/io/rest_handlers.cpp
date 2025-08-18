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
    int id = std::stoi(req->getParameter("id"));
    Json::Value sensor = sensors_.getAsJson(id);
    
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
    error["message"] = "Invalid sensor ID";
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
    int id = std::stoi(req->getParameter("id"));
    
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
    
    if (sensors_.applyPatch(id, patch, applied, err)) {
      Json::Value result;
      result["id"] = id;
      result["applied"] = applied;
      result["sensor"] = sensors_.getAsJson(id);
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
    bool nng_enabled = false;
    bool osc_enabled = false;
    
    for (const auto& sink : config_.sinks) {
      if (sink.type == "nng") {
        nng_enabled = true;
        publishers["nng"]["enabled"] = true;
        publishers["nng"]["url"] = sink.url;
        publishers["nng"]["topic"] = sink.topic;
        publishers["nng"]["encoding"] = sink.encoding;
        publishers["nng"]["rate_limit"] = sink.rate_limit;
      } else if (sink.type == "osc") {
        osc_enabled = true;
        publishers["osc"]["enabled"] = true;
        publishers["osc"]["url"] = sink.url;
        publishers["osc"]["rate_limit"] = sink.rate_limit;
      }
    }
    
    if (!nng_enabled) {
      publishers["nng"]["enabled"] = false;
    }
    if (!osc_enabled) {
      publishers["osc"]["enabled"] = false;
    }
    
    snapshot["publishers"] = publishers;
    
    // UI config
    snapshot["ui"]["ws_listen"] = config_.ui.ws_listen;
    snapshot["ui"]["rest_listen"] = config_.ui.rest_listen;
    
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
