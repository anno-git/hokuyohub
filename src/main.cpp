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

  // Detection pipeline（スタブ）
  SensorManager sensors;
  sensors.configure(appcfg.sensors);
  DBSCAN2D dbscan(appcfg.dbscan_eps, appcfg.dbscan_minPts);

  auto ws = std::make_shared<LiveWs>(bus);
  auto rest = std::make_shared<RestApi>(sensors, dbscan, bus, ws);

  // Drogon routing
  app().setUploadPath("/tmp");
  app().registerController(ws);
  app().registerController(rest);

  // 静的ファイル（webui）
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
    // スタブ：ダミークラスタ
    std::vector<Cluster> items{{7, 3, 0.4f, -1.2f, 0.1f,-1.5f,0.7f,-1.0f,56}};
    ws->pushClustersLite(f.t_ns, f.seq, items);
    bus.publishClusters(f.t_ns, f.seq, items);
  });

  app().run();
  return 0;
}