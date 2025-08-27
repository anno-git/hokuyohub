#include <crow.h>
#include <fstream>
#include <sstream>
#include "config/config.h"
#include "io/rest_handlers.h"
#include "io/ws_handlers.h"
#include "io/publisher_manager.h"
#include "core/sensor_manager.h"
#include "detect/dbscan.h"
#include "detect/prefilter.h"
#include "detect/postfilter.h"
#include "core/filter_manager.h"

int main(int argc, char** argv) {
  std::string cfgPath = "./config/default.yaml";
  std::string httpListen = "";

  for (int i=1;i<argc;++i){
    std::string a = argv[i];
    if(a=="--config" && i+1<argc) cfgPath = argv[++i];
    else if(a=="--listen" && i+1<argc) httpListen = argv[++i];
  }

  AppConfig appcfg = load_app_config(cfgPath);

  // Initialize publisher manager (will be configured via RestApi)
  PublisherManager publisher_manager;
  
  // Detection pipeline with full configuration
  SensorManager sensors(appcfg);
  sensors.configure(appcfg.sensors);
  
  // Initialize DBSCAN with new structured config (fallback to legacy for compatibility)
  const auto& dcfg = appcfg.dbscan;
  float eps_to_use = (dcfg.eps_norm != 2.5f) ? dcfg.eps_norm : dcfg.eps; // Use eps_norm if set, else eps
  DBSCAN2D dbscan(eps_to_use, dcfg.minPts);
  
  // Configure additional parameters
  dbscan.setAngularScale(dcfg.k_scale);
  dbscan.setPerformanceParams(dcfg.h_min, dcfg.h_max, dcfg.R_max, dcfg.M_max);

  // Initialize filter manager with configuration
  FilterManager filterManager(appcfg.prefilter, appcfg.postfilter);

  // Initialize CrowCpp application
  crow::SimpleApp app;
  
  auto ws = std::make_shared<LiveWs>(publisher_manager);
  auto rest = std::make_shared<RestApi>(sensors, filterManager, dbscan, publisher_manager, ws, appcfg);

  ws->setSensorManager(&sensors);
  ws->setFilterManager(&filterManager);
  ws->setAppConfig(&appcfg);
  ws->setDbscan(&dbscan);
  
  // Register routes with CrowCpp app
  rest->registerRoutes(app);
  ws->registerWebSocketRoutes(app);
  
  // Apply initial sink configuration to runtime
  rest->applySinksRuntime();

  // Configure static file serving for webui
  CROW_ROUTE(app, "/")
  ([](const crow::request& req, crow::response& res) {
    res.set_static_file_info("webui/index.html");
    res.end();
  });
  
  // Serve static files from webui directory
  CROW_ROUTE(app, "/<string>")
  ([](const crow::request& req, crow::response& res, const std::string& path) {
    // Basic security check to prevent directory traversal
    if (path.find("..") != std::string::npos) {
      res.code = 400;
      res.body = "Bad Request";
      res.end();
      return;
    }
    
    std::string file_path = "webui/" + path;
    res.set_static_file_info(file_path);
    res.end();
  });

  // Configure HTTP listen address and port
  std::string host = "0.0.0.0";
  uint16_t port = 8080;
  
  auto parseListenAddress = [&](const std::string& url) {
    auto pos = url.find(":");
    if (pos != std::string::npos) {
      host = url.substr(0, pos);
      port = static_cast<uint16_t>(std::stoi(url.substr(pos + 1)));
    }
  };
  
  if (!httpListen.empty()) {
    parseListenAddress(httpListen);
  }
  else if (!appcfg.ui.listen.empty()) {
    parseListenAddress(appcfg.ui.listen);
  }
  
  std::cout << "[App] Starting HTTP server on host:" << host << " port:" << port << std::endl;
  
  // Configure CrowCpp app
  app.bindaddr(host).port(port);


  // センサー開始（スタブ：タイマーでダミーデータを流す）
  sensors.start([&](const ScanFrame& f){
    // Push raw points to WebUI (unfiltered)
    ws->pushRawLite(f.t_ns, f.seq, f.xy, f.sid);
    
    // Apply prefilter through FilterManager
    std::vector<float> filtered_xy = f.xy;
    std::vector<uint8_t> filtered_sid = f.sid;
    
    if (filterManager.isPrefilterEnabled()) {
      try {
        auto filter_result = filterManager.applyPrefilter(f.xy, f.sid);
        filtered_xy = filter_result.xy;
        filtered_sid = filter_result.sid;
        
        // Log filtering statistics
        const auto& stats = filter_result.stats;
      } catch (const std::exception& e) {
        std::cerr << "[Prefilter] Error in frame seq=" << f.seq << ": " << e.what() << std::endl;
        // Continue with unfiltered data
      }
    }
    
    // Apply ROI world_mask filtering after prefilter and before DBSCAN
    if (!appcfg.world_mask.empty()) {
      std::vector<float> roi_filtered_xy;
      std::vector<uint8_t> roi_filtered_sid;
      
      roi_filtered_xy.reserve(filtered_xy.size());
      roi_filtered_sid.reserve(filtered_sid.size());
      
      for (size_t i = 0; i < filtered_xy.size(); i += 2) {
        if (i + 1 < filtered_xy.size()) {
          core::Point2D point(filtered_xy[i], filtered_xy[i + 1]);
          if (appcfg.world_mask.allows(point)) {
            roi_filtered_xy.push_back(filtered_xy[i]);
            roi_filtered_xy.push_back(filtered_xy[i + 1]);
            roi_filtered_sid.push_back(filtered_sid[i / 2]);
          }
        }
      }
      
      filtered_xy = std::move(roi_filtered_xy);
      filtered_sid = std::move(roi_filtered_sid);
    }
    
    // Push filtered points to WebUI
    ws->pushFilteredLite(f.t_ns, f.seq, filtered_xy, filtered_sid);
    
    // DBSCAN clustering on filtered frame
    std::vector<Cluster> raw_clusters;
    try {
      raw_clusters = dbscan.run(filtered_xy, filtered_sid, f.t_ns, f.seq);
    } catch (const std::exception& e) {
      std::cerr << "[DBSCAN] Error in frame seq=" << f.seq << ": " << e.what() << std::endl;
      // Continue with empty items (UI tolerates items.length=0)
    }
    
    // Apply postfilter to clusters through FilterManager
    std::vector<Cluster> final_clusters = raw_clusters;
    if (filterManager.isPostfilterEnabled()) {
      try {
        auto postfilter_result = filterManager.applyPostfilter(raw_clusters, filtered_xy, filtered_sid);
        final_clusters = postfilter_result.clusters;
        
        // Log postfilter statistics
        const auto& stats = postfilter_result.stats;
      } catch (const std::exception& e) {
        std::cerr << "[Postfilter] Error in frame seq=" << f.seq << ": " << e.what() << std::endl;
        // Continue with unfiltered clusters
      }
    }
    
    ws->pushClustersLite(f.t_ns, f.seq, final_clusters);
    publisher_manager.publishClusters(f.t_ns, f.seq, final_clusters);
  });

  // Start the CrowCpp application
  app.run();
  return 0;
}