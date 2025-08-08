#pragma once
#include "sensors/ISensor.h"
#include "config/config.h"
#include <memory>

std::unique_ptr<ISensor> create_sensor(const SensorConfig& cfg);