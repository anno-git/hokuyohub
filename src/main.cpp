#include <drogon/drogon.h>
#include "config/config.h"
#include "io/rest_handlers.h"
#include "io/ws_handlers.h"
#include "io/nng_bus.h"
#include "io/osc_publisher.h"
#include "core/sensor_manager.h"
#include "detect/dbscan.h"
#include "detect/prefilter.h"
#include "detect/postfilter.h"
#include "core/filter_manager.h"

using namespace drogon;

int main(int argc, char** argv) {
  std::string cfgPath = "./config/default.yaml";
  std::string httpListen = "";

  for (int i=1;i<argc;++i){
    std::string a = argv[i];
    if(a=="--config" && i+1<argc) cfgPath = argv[++i];
    else if(a=="--listen" && i+1<argc) httpListen = argv[++i];
  }

  AppConfig appcfg = load_app_config(cfgPath);

  // Initialize publishers (will be configured via RestApi)
  NngBus nng_bus;
  OscPublisher osc_publisher;
  
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

  auto ws = std::make_shared<LiveWs>(nng_bus);
  auto rest = std::make_shared<RestApi>(sensors, filterManager, dbscan, nng_bus, osc_publisher, ws, appcfg);
  app().registerController(ws);
  app().registerController(rest);

  ws->setSensorManager(&sensors);
  ws->setFilterManager(&filterManager);
  ws->setAppConfig(&appcfg);
  
  // Apply initial sink configuration to runtime
  rest->applySinksRuntime();

  app().setUploadPath("/tmp");
  app().setDocumentRoot("./webui");

  // HTTP listen
  auto startHttp = [](const std::string& url) {
    auto pos = url.find(":");
	  if (pos == std::string::npos) return;
    std::string host = url.substr(0, pos);
    uint16_t port = static_cast<uint16_t>(std::stoi(url.substr(pos + 1)));
    std::cout << "[App] Starting HTTP server on host:" << host << " port:" << port << std::endl;
    app().addListener(host, port);
  };
  if (!httpListen.empty()) {
	  startHttp(httpListen);
  }
  else if (!appcfg.ui.listen.empty()) {
    startHttp(appcfg.ui.listen);
  }


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
    nng_bus.publishClusters(f.t_ns, f.seq, final_clusters);
    osc_publisher.publishClusters(f.t_ns, f.seq, final_clusters);
  });

  app().run();
  return 0;
}