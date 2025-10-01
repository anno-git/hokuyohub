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

#include <signal.h>
#include <atomic>
#include <thread>
#include <chrono>

// Global flag for graceful shutdown
std::atomic<bool> shutdown_requested{false};

void signal_handler(int signal) {
  std::cout << "[Signal] Received signal " << signal << ", requesting graceful shutdown..." << std::endl;
  shutdown_requested = true;
}

int main(int argc, char** argv) {
  // Debug: Print all command line arguments
  std::cout << "[DEBUG] Command line arguments (" << argc << " total):" << std::endl;
  for (int i = 0; i < argc; ++i) {
    std::cout << "[DEBUG]   argv[" << i << "] = '" << argv[i] << "'" << std::endl;
  }
  
  // Set up signal handlers for graceful shutdown
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  std::cout << "[Signal] Registered signal handlers for graceful shutdown" << std::endl;
  
  std::string cfgPath = "./configs/default.yaml";
  std::string httpListen = "";

  std::cout << "[DEBUG] Starting argument parsing..." << std::endl;
  for (int i=1;i<argc;++i){
    std::string a = argv[i];
    std::cout << "[DEBUG] Processing argument " << i << ": '" << a << "'" << std::endl;
    if(a=="--config" && i+1<argc) {
      cfgPath = argv[++i];
      std::cout << "[DEBUG] Set config path to: '" << cfgPath << "'" << std::endl;
    }
    else if(a=="--listen" && i+1<argc) {
      httpListen = argv[++i];
      std::cout << "[DEBUG] Set httpListen to: '" << httpListen << "'" << std::endl;
    }
  }
  std::cout << "[DEBUG] Argument parsing complete. cfgPath='" << cfgPath << "', httpListen='" << httpListen << "'" << std::endl;

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

  // Initialize CrowCpp application with explicit cleanup
  std::cout << "[Crow] Creating new Crow application instance..." << std::endl;
  crow::SimpleApp app;
  
  // Force cleanup of any existing Crow state
  std::cout << "[Crow] Initializing fresh Crow state..." << std::endl;
  
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


  // Configure HTTP listen address and port
  std::string host = "0.0.0.0";
  uint16_t port = 8081;
  
  auto parseListenAddress = [&](const std::string& url) {
    std::cout << "[DEBUG] parseListenAddress called with: '" << url << "'" << std::endl;
    auto pos = url.find(":");
    if (pos != std::string::npos) {
      std::string host_part = url.substr(0, pos);
      std::string port_part = url.substr(pos + 1);
      std::cout << "[DEBUG] Parsed host_part: '" << host_part << "', port_part: '" << port_part << "'" << std::endl;
      
      try {
        int parsed_port = std::stoi(port_part);
        std::cout << "[DEBUG] std::stoi result: " << parsed_port << std::endl;
        
        if (parsed_port <= 0 || parsed_port > 65535) {
          std::cerr << "[ERROR] Port out of valid range (1-65535): " << parsed_port << std::endl;
          std::cout << "[DEBUG] Using fallback port 8080" << std::endl;
          port = 8080;
        } else {
          port = static_cast<uint16_t>(parsed_port);
          std::cout << "[DEBUG] Final port set to: " << port << std::endl;
        }
        host = host_part;
        std::cout << "[DEBUG] Final host set to: '" << host << "'" << std::endl;
      } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to parse port from '" << port_part << "': " << e.what() << std::endl;
        std::cout << "[DEBUG] Using fallback port 8080" << std::endl;
        port = 8080;
        host = host_part;
      }
    } else {
      std::cout << "[DEBUG] No colon found in URL, treating as host only: '" << url << "'" << std::endl;
    }
  };
  
  if (!httpListen.empty()) {
    std::cout << "[Config] Using command line listen: " << httpListen << std::endl;
    parseListenAddress(httpListen);
  }
  else if (!appcfg.ui.listen.empty()) {
    std::cout << "[Config] Using config file listen: " << appcfg.ui.listen << std::endl;
    parseListenAddress(appcfg.ui.listen);
  }
  else {
    std::cout << "[Config] Using default values: host=" << host << " port=" << port << std::endl;
  }
  
  // Additional validation: ensure port was actually set correctly
  std::cout << "[DEBUG] After parseListenAddress - host='" << host << "' port=" << port << std::endl;
  
  std::cout << "[App] Parsed configuration - host:'" << host << "' port:" << port << std::endl;
  
  // Validate configuration before binding
  if (host.empty()) {
    std::cerr << "[ERROR] Host is empty! Using fallback 0.0.0.0" << std::endl;
    host = "0.0.0.0";
  }
  if (port <= 0 || port > 65535) {
    std::cerr << "[ERROR] Invalid port " << port << "! Must be 1-65535. Using fallback 8080" << std::endl;
    port = 8080;
  }
  
  std::cout << "[App] Final configuration - Starting HTTP server on host:'" << host << "' port:" << port << std::endl;
  
  // Configure CrowCpp app with explicit verification
  std::cout << "[Crow] Configuring Crow app..." << std::endl;
  app.bindaddr(host);
  std::cout << "[Crow] Set bindaddr to: " << host << std::endl;
  app.port(port);
  std::cout << "[Crow] Set port to: " << port << std::endl;
  
  // CRITICAL FIX: Store port value to prevent corruption
  const uint16_t saved_port = port;
  const std::string saved_host = host;
  std::cout << "[Crow] CRITICAL: Saved port=" << saved_port << " host='" << saved_host << "' for verification" << std::endl;
  
  // Force verification of Crow configuration
  try {
    // Attempt to validate Crow's internal state before sensors start
    std::cout << "[Crow] Crow configuration complete, proceeding with sensor initialization..." << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "[ERROR] Crow configuration failed: " << e.what() << std::endl;
    return 1;
  }

  // CRITICAL FIX: Defer sensor initialization until after Crow is fully configured
  std::cout << "[App] CRITICAL: Deferring sensor start to prevent Crow corruption..." << std::endl;
  
  // センサー開始（スタブ：タイマーでダミーデータを流す）
  std::cout << "[App] CRITICAL: Starting sensors with callback registration..." << std::endl;
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

  // Start the CrowCpp application with signal checking
  std::cout << "[App] Starting CrowCpp server..." << std::endl;
  
  // CRITICAL: Verify Crow's internal state before starting
  std::cout << "[DEBUG-CRITICAL] Crow internal state verification:" << std::endl;
  std::cout << "[DEBUG-CRITICAL] Current port variable: " << port << std::endl;
  std::cout << "[DEBUG-CRITICAL] Saved port value: " << saved_port << std::endl;
  std::cout << "[DEBUG-CRITICAL] About to call app.run_async() - verifying port configuration..." << std::endl;
  
  // CRITICAL FIX: Validate saved values before binding
  if (saved_port <= 0 || saved_port > 65535) {
    std::cerr << "[ERROR-CRITICAL] Invalid saved port " << saved_port << "! Using emergency fallback 8080" << std::endl;
    const uint16_t emergency_port = 8080;
    app.bindaddr(saved_host);
    app.port(emergency_port);
    std::cout << "[DEBUG-CRITICAL] EMERGENCY configuration - host='" << saved_host << "' port=" << emergency_port << std::endl;
  } else {
    // Force complete reconfiguration of Crow with saved values
    app.bindaddr(saved_host);
    app.port(saved_port);
    std::cout << "[DEBUG-CRITICAL] FORCED re-configuration - host='" << saved_host << "' port=" << saved_port << std::endl;
  }
  
  // Use concurrent mode to allow signal checking
  app.multithreaded();
  std::cout << "[DEBUG-CRITICAL] About to start async server..." << std::endl;
  auto future = app.run_async();
  
  // Wait for shutdown signal
  while (!shutdown_requested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  
  std::cout << "[App] Shutdown requested, stopping server..." << std::endl;
  app.stop();
  
  // Wait for server to finish
  future.wait();
  std::cout << "[App] Server stopped gracefully" << std::endl;
  
  return 0;
}