#include "config.h"
#include <yaml-cpp/yaml.h>
#include <algorithm>

static float clampf(float v, float lo, float hi){ return std::max(lo, std::min(hi, v)); }

AppConfig load_app_config(const std::string& path){
  AppConfig cfg;
  YAML::Node y = YAML::LoadFile(path);

  if (y["sensors"] && y["sensors"].IsSequence()) {
    for (const auto& s : y["sensors"]) {
      SensorConfig c;

      if (s["id"])        c.id        = s["id"].as<std::string>(c.id);
	  if (s["type"])      c.type      = s["type"].as<std::string>(c.type);
      if (s["name"])      c.name      = s["name"].as<std::string>(c.name);
      if (s["endpoint"]) {
		std::string endpoint = s["endpoint"].as<std::string>("");
		// split "host:port" into host and port
		auto colon_pos = endpoint.find(":");
		if (colon_pos != std::string::npos) {
		  c.host = endpoint.substr(0, colon_pos);
		  c.port = std::stoi(endpoint.substr(colon_pos + 1));
		} else {
		  c.host = endpoint;
		}
      }
      if (s["enabled"])   c.enabled   = s["enabled"].as<bool>(c.enabled);

      if (s["mode"])      c.mode      = s["mode"].as<std::string>(c.mode);
      if (s["interval"])  c.interval  = std::max(0, s["interval"].as<int>(c.interval));
      if (s["skip_step"]) c.skip_step = std::max(0, s["skip_step"].as<int>(c.skip_step)); // ← 追加

	  if (s["ignore_checkSumError"]) {
        int v = s["ignore_checkSumError"].as<int>(1);
        c.ignore_checksum_error = (v != 0) ? 1 : 0;
      } else {
        c.ignore_checksum_error = 1;
      }

      if (auto p = s["pose"]) {
        if (p["tx"])    c.pose.tx        = p["tx"].as<float>(c.pose.tx);
        if (p["ty"])    c.pose.ty        = p["ty"].as<float>(c.pose.ty);
        if (p["theta"]) c.pose.theta_deg = p["theta"].as<float>(c.pose.theta_deg); // degree
      }

      if (auto m = s["mask"]) {
        if (auto a = m["angle"]) {
          if (a["min"]) c.mask.angle.min_deg = a["min"].as<float>(c.mask.angle.min_deg);
          if (a["max"]) c.mask.angle.max_deg = a["max"].as<float>(c.mask.angle.max_deg);
          if (c.mask.angle.min_deg > c.mask.angle.max_deg)
            std::swap(c.mask.angle.min_deg, c.mask.angle.max_deg);
          c.mask.angle.min_deg = clampf(c.mask.angle.min_deg, -180.0f, 180.0f);
          c.mask.angle.max_deg = clampf(c.mask.angle.max_deg, -180.0f, 180.0f);
        }
        if (auto r = m["range"]) {
          if (r["near"]) c.mask.range.near_m = std::max(0.0f, r["near"].as<float>(c.mask.range.near_m));
          if (r["far"])  c.mask.range.far_m  = std::max(0.0f, r["far"].as<float>(c.mask.range.far_m));
          if (c.mask.range.near_m > c.mask.range.far_m)
            std::swap(c.mask.range.near_m, c.mask.range.far_m);
        }
      }

      cfg.sensors.push_back(std::move(c));
    }
  }

  if (auto d = y["dbscan"]) {
    if (d["eps"])    cfg.dbscan_eps    = d["eps"].as<float>(cfg.dbscan_eps);
    if (d["minPts"]) cfg.dbscan_minPts = d["minPts"].as<int>(cfg.dbscan_minPts);
  }

  if (auto u = y["ui"]) {
    if (u["ws_listen"])   cfg.ui.ws_listen   = u["ws_listen"].as<std::string>(cfg.ui.ws_listen);
    if (u["rest_listen"]) cfg.ui.rest_listen = u["rest_listen"].as<std::string>(cfg.ui.rest_listen);
  }

  if (y["sinks"] && y["sinks"].IsSequence()) {
    for (const auto& sn : y["sinks"]) {
      SinkConfig sc;
      if (sn["type"])      sc.type      = sn["type"].as<std::string>("");
      if (sn["url"])       sc.url       = sn["url"].as<std::string>("");
      if (sn["topic"])     sc.topic     = sn["topic"].as<std::string>("");
      if (sn["encoding"])  sc.encoding  = sn["encoding"].as<std::string>("");
      if (sn["rate_limit"])sc.rate_limit= sn["rate_limit"].as<int>(0);
      cfg.sinks.push_back(std::move(sc));
    }
  }

  return cfg;
}