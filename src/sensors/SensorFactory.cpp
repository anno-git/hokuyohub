#include "SensorFactory.h"
#include "sensors/hokuyo/HokuyoSensorUrg.h"

std::unique_ptr<ISensor> create_sensor(const SensorConfig& cfg) {
    if (cfg.type == "hokuyo_eth" || cfg.type == "hokuyo_urg_eth") {
        return std::make_unique<HokuyoSensorUrg>(cfg.id);
    }
    // TODO: 他機種/モック
    return nullptr;
}