#include "sensor_manager.h"
#include <chrono>
#include <thread>

void SensorManager::configure(const std::vector<SensorCfg>&){ /* TODO: 実装 */ }
void SensorManager::setSensorPower(int, bool){ /* TODO */ }
void SensorManager::setPose(int, float, float, float){ /* TODO */ }
void SensorManager::setSensorMask(int, const SensorMaskLocal&){ /* TODO */ }

void SensorManager::start(FrameCallback cb){
  // MVP スタブ：60Hzでダミーフレームを生成
  std::thread([cb]{
    using namespace std::chrono;
    uint32_t seq=0; auto t0=steady_clock::now();
    while(true){
      ScanFrame f; f.seq=seq++;
      f.t_ns = duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
      cb(f);
      std::this_thread::sleep_until(t0 + duration<double>(seq/60.0));
    }
  }).detach();
}