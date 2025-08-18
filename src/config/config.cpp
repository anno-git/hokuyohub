#include "config.h"
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <iostream>

static float clampf(float v, float lo, float hi){ return std::max(lo, std::min(hi, v)); }

static std::vector<core::Point2D> parsePointList(const YAML::Node& pts) {
  std::vector<core::Point2D> out;
  if (!pts || !pts.IsSequence()) return out;
  out.reserve(pts.size());
  for (const auto& n : pts) {
    if (n.IsSequence() && n.size() >= 2) {
      out.push_back({n[0].as<double>(), n[1].as<double>()});
    }
  }
  return out;
}

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
      if (s["skip_step"]) c.skip_step = std::max(0, s["skip_step"].as<int>(c.skip_step));

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
    if (d["eps_norm"]) cfg.dbscan.eps_norm = d["eps_norm"].as<float>(cfg.dbscan.eps_norm);
    if (d["minPts"])   cfg.dbscan.minPts   = std::max(1, d["minPts"].as<int>(cfg.dbscan.minPts));
    if (d["k_scale"])  cfg.dbscan.k_scale  = std::max(0.1f, d["k_scale"].as<float>(cfg.dbscan.k_scale));
    
    // Performance parameters
    if (d["h_min"])    cfg.dbscan.h_min    = std::max(0.001f, d["h_min"].as<float>(cfg.dbscan.h_min));
    if (d["h_max"])    cfg.dbscan.h_max    = std::max(cfg.dbscan.h_min, d["h_max"].as<float>(cfg.dbscan.h_max));
    if (d["R_max"])    cfg.dbscan.R_max    = std::max(1, d["R_max"].as<int>(cfg.dbscan.R_max));
    if (d["M_max"])    cfg.dbscan.M_max    = std::max(10, d["M_max"].as<int>(cfg.dbscan.M_max));
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
    if (u["listen"])   cfg.ui.listen   = u["listen"].as<std::string>(cfg.ui.listen);
  }

  // Security configuration
  if (auto sec = y["security"]) {
    if (sec["api_token"]) {
      cfg.security.api_token = sec["api_token"].as<std::string>("");
    }
  }

  // World mask configuration
  if (auto wm = y["world_mask"]) {
    if (auto inc = wm["include"]) {
      for (const auto& polyNode : inc) {
        core::Polygon poly;
        poly.points = parsePointList(polyNode);
        if (!poly.points.empty()) {
          cfg.world_mask.include.push_back(std::move(poly));
        }
      }
    }
    if (auto exc = wm["exclude"]) {
      for (const auto& polyNode : exc) {
        core::Polygon poly;
        poly.points = parsePointList(polyNode);
        if (!poly.points.empty()) {
          cfg.world_mask.exclude.push_back(std::move(poly));
        }
      }
    }
  }

  if (y["sinks"] && y["sinks"].IsSequence()) {
    for (const auto& sn : y["sinks"]) {
      SinkConfig sc;

      std::string type = sn["type"].as<std::string>("");
      if (sn["topic"])     sc.topic     = sn["topic"].as<std::string>("");
      if (sn["rate_limit"])sc.rate_limit= sn["rate_limit"].as<int>(0);

      if(type == "nng") {
        sc.cfg = NngConfig{};
        if (sn["url"])       sc.nng().url       = sn["url"].as<std::string>("");
        if (sn["encoding"])  sc.nng().encoding  = sn["encoding"].as<std::string>("");
      }
      if(type == "osc") {
        sc.cfg = OscConfig{};
        if (sn["url"])       sc.osc().url       = sn["url"].as<std::string>("");
        if (sn["in_bundle"]) sc.osc().in_bundle = sn["in_bundle"].as<bool>(false);
        if (sn["bundle_fragment_size"]) sc.osc().bundle_fragment_size = sn["bundle_fragment_size"].as<int>(0);
      }

      cfg.sinks.push_back(std::move(sc));
    }
  }

  return cfg;
}

std::string dump_app_config(const AppConfig& cfg) {
  YAML::Emitter out;
  out << YAML::BeginMap;

  // Sensors
  out << YAML::Key << "sensors" << YAML::Value << YAML::BeginSeq;
  for (const auto& s : cfg.sensors) {
    out << YAML::BeginMap;
    out << YAML::Key << "id" << YAML::Value << s.id;
    out << YAML::Key << "type" << YAML::Value << s.type;
    out << YAML::Key << "name" << YAML::Value << s.name;
    out << YAML::Key << "endpoint" << YAML::Value << (s.host + ":" + std::to_string(s.port));
    out << YAML::Key << "enabled" << YAML::Value << s.enabled;
    out << YAML::Key << "mode" << YAML::Value << s.mode;
    out << YAML::Key << "interval" << YAML::Value << s.interval;
    out << YAML::Key << "skip_step" << YAML::Value << s.skip_step;
    out << YAML::Key << "ignore_checkSumError" << YAML::Value << s.ignore_checksum_error;
    
    out << YAML::Key << "pose" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "tx" << YAML::Value << s.pose.tx;
    out << YAML::Key << "ty" << YAML::Value << s.pose.ty;
    out << YAML::Key << "theta" << YAML::Value << s.pose.theta_deg;
    out << YAML::EndMap;
    
    out << YAML::Key << "mask" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "angle" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "min" << YAML::Value << s.mask.angle.min_deg;
    out << YAML::Key << "max" << YAML::Value << s.mask.angle.max_deg;
    out << YAML::EndMap;
    out << YAML::Key << "range" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "near" << YAML::Value << s.mask.range.near_m;
    out << YAML::Key << "far" << YAML::Value << s.mask.range.far_m;
    out << YAML::EndMap;
    out << YAML::EndMap;
    
    out << YAML::EndMap;
  }
  out << YAML::EndSeq;

  // DBSCAN
  out << YAML::Key << "dbscan" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "eps_norm" << YAML::Value << cfg.dbscan.eps_norm;
  out << YAML::Key << "minPts" << YAML::Value << cfg.dbscan.minPts;
  out << YAML::Key << "k_scale" << YAML::Value << cfg.dbscan.k_scale;
  out << YAML::Key << "h_min" << YAML::Value << cfg.dbscan.h_min;
  out << YAML::Key << "h_max" << YAML::Value << cfg.dbscan.h_max;
  out << YAML::Key << "R_max" << YAML::Value << cfg.dbscan.R_max;
  out << YAML::Key << "M_max" << YAML::Value << cfg.dbscan.M_max;
  out << YAML::EndMap;

  // Prefilter
  out << YAML::Key << "prefilter" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "enabled" << YAML::Value << cfg.prefilter.enabled;
  
  out << YAML::Key << "neighborhood" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "enabled" << YAML::Value << cfg.prefilter.neighborhood.enabled;
  out << YAML::Key << "k" << YAML::Value << cfg.prefilter.neighborhood.k;
  out << YAML::Key << "r_base" << YAML::Value << cfg.prefilter.neighborhood.r_base;
  out << YAML::Key << "r_scale" << YAML::Value << cfg.prefilter.neighborhood.r_scale;
  out << YAML::EndMap;
  
  out << YAML::Key << "spike_removal" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "enabled" << YAML::Value << cfg.prefilter.spike_removal.enabled;
  out << YAML::Key << "dr_threshold" << YAML::Value << cfg.prefilter.spike_removal.dr_threshold;
  out << YAML::Key << "window_size" << YAML::Value << cfg.prefilter.spike_removal.window_size;
  out << YAML::EndMap;
  
  out << YAML::Key << "outlier_removal" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "enabled" << YAML::Value << cfg.prefilter.outlier_removal.enabled;
  out << YAML::Key << "median_window" << YAML::Value << cfg.prefilter.outlier_removal.median_window;
  out << YAML::Key << "outlier_threshold" << YAML::Value << cfg.prefilter.outlier_removal.outlier_threshold;
  out << YAML::Key << "use_robust_regression" << YAML::Value << cfg.prefilter.outlier_removal.use_robust_regression;
  out << YAML::EndMap;
  
  out << YAML::Key << "intensity_filter" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "enabled" << YAML::Value << cfg.prefilter.intensity_filter.enabled;
  out << YAML::Key << "min_intensity" << YAML::Value << cfg.prefilter.intensity_filter.min_intensity;
  out << YAML::Key << "min_reliability" << YAML::Value << cfg.prefilter.intensity_filter.min_reliability;
  out << YAML::EndMap;
  
  out << YAML::Key << "isolation_removal" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "enabled" << YAML::Value << cfg.prefilter.isolation_removal.enabled;
  out << YAML::Key << "min_cluster_size" << YAML::Value << cfg.prefilter.isolation_removal.min_cluster_size;
  out << YAML::Key << "isolation_radius" << YAML::Value << cfg.prefilter.isolation_removal.isolation_radius;
  out << YAML::EndMap;
  
  out << YAML::EndMap;

  // Postfilter
  out << YAML::Key << "postfilter" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "enabled" << YAML::Value << cfg.postfilter.enabled;
  
  out << YAML::Key << "isolation_removal" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "enabled" << YAML::Value << cfg.postfilter.isolation_removal.enabled;
  out << YAML::Key << "min_points_size" << YAML::Value << cfg.postfilter.isolation_removal.min_points_size;
  out << YAML::Key << "isolation_radius" << YAML::Value << cfg.postfilter.isolation_removal.isolation_radius;
  out << YAML::Key << "required_neighbors" << YAML::Value << cfg.postfilter.isolation_removal.required_neighbors;
  out << YAML::EndMap;
  
  out << YAML::EndMap;

  // UI
  out << YAML::Key << "ui" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "listen" << YAML::Value << cfg.ui.listen;
  out << YAML::EndMap;

  // Security
  out << YAML::Key << "security" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "api_token" << YAML::Value << cfg.security.api_token;
  out << YAML::EndMap;

  // World mask
  out << YAML::Key << "world_mask" << YAML::Value << YAML::BeginMap;
  
  auto emitPolygons = [&](const char* key, const std::vector<core::Polygon>& polys) {
    out << YAML::Key << key << YAML::Value << YAML::BeginSeq;
    for (const auto& poly : polys) {
      out << YAML::BeginSeq;
      for (const auto& pt : poly.points) {
        out << YAML::Flow << YAML::BeginSeq << pt.x << pt.y << YAML::EndSeq;
      }
      out << YAML::EndSeq;
    }
    out << YAML::EndSeq;
  };
  
  emitPolygons("include", cfg.world_mask.include);
  emitPolygons("exclude", cfg.world_mask.exclude);
  
  out << YAML::EndMap;

  // Sinks
  out << YAML::Key << "sinks" << YAML::Value << YAML::BeginSeq;
  for (const auto& sink : cfg.sinks) {
    out << YAML::BeginMap;
    if (sink.isOsc()) {
      auto cfg = sink.osc();
      out << YAML::Key << "type" << YAML::Value << "osc";
      out << YAML::Key << "url" << YAML::Value << cfg.url;
      out << YAML::Key << "in_bundle" << YAML::Value << cfg.in_bundle;
      out << YAML::Key << "bundle_fragment_size" << YAML::Value << cfg.bundle_fragment_size;
    } else if (sink.isNng()) {
      auto cfg = sink.nng();
      out << YAML::Key << "type" << YAML::Value << "nng";
      out << YAML::Key << "url" << YAML::Value << cfg.url;
      out << YAML::Key << "encoding" << YAML::Value << cfg.encoding;
    }
    out << YAML::Key << "topic" << YAML::Value << sink.topic;
    out << YAML::Key << "rate_limit" << YAML::Value << sink.rate_limit;
    out << YAML::EndMap;
  }
  out << YAML::EndSeq;

  out << YAML::EndMap;
  return std::string(out.c_str());
}