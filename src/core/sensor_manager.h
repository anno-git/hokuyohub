#pragma once

#include <functional>
#include <vector>
#include <cstdint>
#include "config/config.h"
#include <json/json.h>

/**
 * センサーID管理の整理:
 *
 * 1. 点群処理用: 数値センサーID (sid)
 *    - 型: uint8_t (0-255)
 *    - 用途: ScanFrame.sid, フィルター処理, クラスタリング
 *    - 高速な点群処理のため数値で管理
 *
 * 2. 設定・API用: 文字列センサーID
 *    - 型: std::string (例: "sensor_1", "lidar_front")
 *    - 用途: SensorConfig.id, REST API削除, 設定ファイル
 *    - 人間が読みやすい識別子
 *
 * 3. API操作用: スロットインデックス
 *    - 型: int (0から始まる配列インデックス)
 *    - 用途: REST API操作, WebSocket操作
 *    - 内部的なセンサー管理用
 *
 * 変換: SensorManager内でid2sidマップにより文字列ID↔数値sidを変換
 */

struct ScanFrame {
  uint64_t t_ns; uint32_t seq;
  std::vector<float> xy;           // [x0,y0,x1,y1,...]
  std::vector<uint8_t> sid;        // 点群処理用の数値センサーID (0-255)
};

class SensorManager {
public:
  using FrameCallback = std::function<void(const ScanFrame&)>;
  
  // Constructor to accept AppConfig reference
  SensorManager(AppConfig& app_config);
  
  void configure(const std::vector<SensorConfig>& cfgs);
  void start(FrameCallback cb);
  void setSensorPower(std::string sensor_id, bool on);
  void setPose(std::string sensor_id, float tx, float ty, float theta_deg);
  void setSensorMask(std::string sensor_id, const SensorMaskLocal& m);

  bool setEnabled(std::string sensor_id, bool on);
  Json::Value getAsJson(std::string sensor_id) const;
  Json::Value listAsJson() const;

  bool applyPatch(std::string id, const Json::Value& patch, Json::Value& applied, std::string& err);
  bool restartSensor(std::string sensor_id);
  
  // Reload configuration from AppConfig (for Load/Import operations)
  void reloadFromAppConfig();

private:
  AppConfig& app_config_;  // Reference to main config for immediate updates
};
