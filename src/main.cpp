#include <drogon/drogon.h>
#include "config/config.h"
#include "io/rest_handlers.h"
#include "io/ws_handlers.h"
#include "io/nng_bus.h"
#include "core/sensor_manager.h"
#include "detect/dbscan.h"

using namespace drogon;

int main(int argc, char** argv) {
  std::string cfgPath = "./config/default.yaml";
  std::string httpListen = "";
  std::string pubUrl = "tcp://0.0.0.0:5555";

  for (int i=1;i<argc;++i){
    std::string a = argv[i];
    if(a=="--config" && i+1<argc) cfgPath = argv[++i];
    else if(a=="--listen" && i+1<argc) httpListen = argv[++i];
    else if(a=="--pub" && i+1<argc) pubUrl = argv[++i];
  }

  AppConfig appcfg = load_app_config(cfgPath);

  for (const auto& s : appcfg.sinks) {
    if (s.type == "nng" && !s.url.empty()) {
      pubUrl = s.url;
      break;
    }
  }

  // NNG publisher（存在しない場合は NO-OP）
  NngBus bus; bus.startPublisher(pubUrl);

  // Detection pipeline with full configuration
  SensorManager sensors;
  sensors.configure(appcfg.sensors);
  
  // Initialize DBSCAN with new structured config (fallback to legacy for compatibility)
  const auto& dcfg = appcfg.dbscan;
  float eps_to_use = (dcfg.eps_norm != 2.5f) ? dcfg.eps_norm : dcfg.eps; // Use eps_norm if set, else eps
  DBSCAN2D dbscan(eps_to_use, dcfg.minPts);
  
  // Configure additional parameters
  dbscan.setAngularScale(dcfg.k_scale);
  dbscan.setPerformanceParams(dcfg.h_min, dcfg.h_max, dcfg.R_max, dcfg.M_max);

  auto ws = std::make_shared<LiveWs>(bus);
  auto rest = std::make_shared<RestApi>(sensors, dbscan, bus, ws);
  app().registerController(ws);
  app().registerController(rest);

  ws->setSensorManager(&sensors);

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
  else if (!appcfg.ui.rest_listen.empty()) {
    startHttp(appcfg.ui.rest_listen);
  }


  // センサー開始（スタブ：タイマーでダミーデータを流す）
  sensors.start([&](const ScanFrame& f){
    // Push raw points to WebUI
    ws->pushRawLite(f.t_ns, f.seq, f.xy, f.sid);
    
    // DBSCAN clustering on aggregated frame
    std::vector<Cluster> items;
    try {
      items = dbscan.run(f.xy, f.sid, f.t_ns, f.seq);
    } catch (const std::exception& e) {
      std::cerr << "[DBSCAN] Error in frame seq=" << f.seq << ": " << e.what() << std::endl;
      // Continue with empty items (UI tolerates items.length=0)
    }
    ws->pushClustersLite(f.t_ns, f.seq, items);
    bus.publishClusters(f.t_ns, f.seq, items);
  });

  app().run();
  return 0;
}