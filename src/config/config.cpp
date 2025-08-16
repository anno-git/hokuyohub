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
    // Legacy fields for backward compatibility
    if (d["eps"])    cfg.dbscan_eps    = d["eps"].as<float>(cfg.dbscan_eps);
    if (d["minPts"]) cfg.dbscan_minPts = d["minPts"].as<int>(cfg.dbscan_minPts);
    
    // New structured config
    if (d["eps"])      cfg.dbscan.eps      = d["eps"].as<float>(cfg.dbscan.eps);
    if (d["eps_norm"]) cfg.dbscan.eps_norm = d["eps_norm"].as<float>(cfg.dbscan.eps_norm);
    if (d["minPts"])   cfg.dbscan.minPts   = std::max(1, d["minPts"].as<int>(cfg.dbscan.minPts));
    if (d["k_scale"])  cfg.dbscan.k_scale  = std::max(0.1f, d["k_scale"].as<float>(cfg.dbscan.k_scale));
    
    // Performance parameters
    if (d["h_min"])    cfg.dbscan.h_min    = std::max(0.001f, d["h_min"].as<float>(cfg.dbscan.h_min));
    if (d["h_max"])    cfg.dbscan.h_max    = std::max(cfg.dbscan.h_min, d["h_max"].as<float>(cfg.dbscan.h_max));
    if (d["R_max"])    cfg.dbscan.R_max    = std::max(1, d["R_max"].as<int>(cfg.dbscan.R_max));
    if (d["M_max"])    cfg.dbscan.M_max    = std::max(10, d["M_max"].as<int>(cfg.dbscan.M_max));
    
    // If eps_norm not explicitly set, use eps as eps_norm for backward compatibility
    if (!d["eps_norm"] && d["eps"]) {
      cfg.dbscan.eps_norm = cfg.dbscan.eps;
    }
  }

  // Prefilter configuration
  if (auto p = y["prefilter"]) {
    if (p["enabled"]) cfg.prefilter.enabled = p["enabled"].as<bool>(cfg.prefilter.enabled);
    
    // Neighborhood filter
    if (auto n = p["neighborhood"]) {
      if (n["enabled"])  cfg.prefilter.neighborhood.enabled  = n["enabled"].as<bool>(cfg.prefilter.neighborhood.enabled);
      if (n["k"])        cfg.prefilter.neighborhood.k        = std::max(1, n["k"].as<int>(cfg.prefilter.neighborhood.k));
      if (n["r_base"])   cfg.prefilter.neighborhood.r_base   = std::max(0.001f, n["r_base"].as<float>(cfg.prefilter.neighborhood.r_base));
      if (n["r_scale"])  cfg.prefilter.neighborhood.r_scale  = std::max(0.0f, n["r_scale"].as<float>(cfg.prefilter.neighborhood.r_scale));
    }
    
    // Spike removal filter
    if (auto s = p["spike_removal"]) {
      if (s["enabled"])      cfg.prefilter.spike_removal.enabled      = s["enabled"].as<bool>(cfg.prefilter.spike_removal.enabled);
      if (s["dr_threshold"]) cfg.prefilter.spike_removal.dr_threshold = std::max(0.0f, s["dr_threshold"].as<float>(cfg.prefilter.spike_removal.dr_threshold));
      if (s["window_size"])  cfg.prefilter.spike_removal.window_size  = std::max(1, s["window_size"].as<int>(cfg.prefilter.spike_removal.window_size));
    }
    
    // Outlier removal filter
    if (auto o = p["outlier_removal"]) {
      if (o["enabled"])           cfg.prefilter.outlier_removal.enabled           = o["enabled"].as<bool>(cfg.prefilter.outlier_removal.enabled);
      if (o["median_window"])     cfg.prefilter.outlier_removal.median_window     = std::max(1, o["median_window"].as<int>(cfg.prefilter.outlier_removal.median_window));
      if (o["outlier_threshold"]) cfg.prefilter.outlier_removal.outlier_threshold = std::max(0.1f, o["outlier_threshold"].as<float>(cfg.prefilter.outlier_removal.outlier_threshold));
      if (o["use_robust_regression"]) cfg.prefilter.outlier_removal.use_robust_regression = o["use_robust_regression"].as<bool>(cfg.prefilter.outlier_removal.use_robust_regression);
    }
    
    // Intensity filter
    if (auto i = p["intensity_filter"]) {
      if (i["enabled"])        cfg.prefilter.intensity_filter.enabled        = i["enabled"].as<bool>(cfg.prefilter.intensity_filter.enabled);
      if (i["min_intensity"])  cfg.prefilter.intensity_filter.min_intensity  = std::max(0.0f, i["min_intensity"].as<float>(cfg.prefilter.intensity_filter.min_intensity));
      if (i["min_reliability"]) cfg.prefilter.intensity_filter.min_reliability = std::max(0.0f, i["min_reliability"].as<float>(cfg.prefilter.intensity_filter.min_reliability));
    }
    
    // Isolation removal filter
    if (auto iso = p["isolation_removal"]) {
      if (iso["enabled"])          cfg.prefilter.isolation_removal.enabled          = iso["enabled"].as<bool>(cfg.prefilter.isolation_removal.enabled);
      if (iso["min_cluster_size"]) cfg.prefilter.isolation_removal.min_cluster_size = std::max(1, iso["min_cluster_size"].as<int>(cfg.prefilter.isolation_removal.min_cluster_size));
      if (iso["isolation_radius"]) cfg.prefilter.isolation_removal.isolation_radius = std::max(0.001f, iso["isolation_radius"].as<float>(cfg.prefilter.isolation_removal.isolation_radius));
    }
  }

  // Postfilter configuration
  if (auto p = y["postfilter"]) {
    if (p["enabled"]) cfg.postfilter.enabled = p["enabled"].as<bool>(cfg.postfilter.enabled);
    
    // Isolation removal filter
    if (auto iso = p["isolation_removal"]) {
      if (iso["enabled"])          cfg.postfilter.isolation_removal.enabled          = iso["enabled"].as<bool>(cfg.postfilter.isolation_removal.enabled);
      if (iso["min_points_size"]) cfg.postfilter.isolation_removal.min_points_size = std::max(1, iso["min_points_size"].as<int>(cfg.postfilter.isolation_removal.min_points_size));
      if (iso["isolation_radius"]) cfg.postfilter.isolation_removal.isolation_radius = std::max(0.001f, iso["isolation_radius"].as<float>(cfg.postfilter.isolation_removal.isolation_radius));
      if (iso["required_neighbors"]) cfg.postfilter.isolation_removal.required_neighbors = std::max(1, iso["required_neighbors"].as<int>(cfg.postfilter.isolation_removal.required_neighbors));
    }
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