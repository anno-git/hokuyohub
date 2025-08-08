#include "config.h"
#include <yaml-cpp/yaml.h>

AppConfig load_app_config(const std::string& path){
  AppConfig cfg;
  auto y = YAML::LoadFile(path);
  for(auto s : y["sensors"]){
    SensorCfg c;
    c.id = s["id"].as<int>();
    c.name = s["name"].as<std::string>("sensor");
    c.transport = s["transport"].as<std::string>("tcp");
    c.endpoint = s["endpoint"].as<std::string>("");
    c.enabled = s["enabled"].as<bool>(true);
    auto p = s["pose"]; c.pose = {p["tx"].as<float>(0), p["ty"].as<float>(0), p["theta"].as<float>(0)};
    auto m = s["mask"]; c.mask = {m["angle"]["min"].as<float>(-3.14f), m["angle"]["max"].as<float>(3.14f), m["range"]["near"].as<float>(0.05f), m["range"]["far"].as<float>(15.0f)};
    cfg.sensors.push_back(c);
  }
  if(y["dbscan"]) {
    cfg.dbscan_eps = y["dbscan"]["eps"].as<float>(0.12f);
    cfg.dbscan_minPts = y["dbscan"]["minPts"].as<int>(6);
  }
  return cfg;
}