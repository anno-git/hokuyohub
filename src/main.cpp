#include <drogon/drogon.h>
#include "config/config.hpp"
#include "io/rest_handlers.hpp"
#include "io/ws_handlers.hpp"
#include "io/nng_bus.hpp"
#include "core/sensor_manager.hpp"
#include "detect/dbscan.hpp"

using namespace drogon;

int main(int argc, char** argv) {
  std::string cfgPath = "./default.yaml";
  std::string httpListen = "0.0.0.0:8080";
  std::string pubUrl = "tcp://0.0.0.0:5555";

  for (int i=1;i<argc;++i){
    std::string a = argv[i];
    if(a=="--config" && i+1<argc) cfgPath = argv[++i];
    else if(a=="--listen" && i+1<argc) httpListen = argv[++i];
    else if(a=="--pub" && i+1<argc) pubUrl = argv[++i];
  }

  AppConfig appcfg = load_app_config(cfgPath);

  // NNG publisher（存在しない場合は NO-OP）
  NngBus bus; bus.startPublisher(pubUrl);

  // Detection pipeline（スタブ）
  SensorManager sensors;
  sensors.configure(appcfg.sensors);
  DBSCAN2D dbscan(appcfg.dbscan_eps, appcfg.dbscan_minPts);

  auto ws = std::make_shared<LiveWs>(bus);
  auto rest = std::make_shared<RestApi>(sensors, dbscan, bus, ws);

  // Drogon routing
  app().registerController(ws);
  app().registerController(rest);

  // 静的ファイル（webui）
  app().setDocumentRoot(".");

  // HTTP listen
  auto pos = httpListen.find(":");
  std::string host = httpListen.substr(0,pos);
  uint16_t port = static_cast<uint16_t>(std::stoi(httpListen.substr(pos+1)));
  app().addListener(host, port);

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