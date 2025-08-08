#pragma once
#include <string>
#include <vector>

// 将来ここに ETH(TCP) の SCIP 実装を配置。
// 非同期受信（ASIO）で 1 センサー = 1 スレッド / 1 コネクション を想定。

struct RawScan { /* 生データ格納予定 */ };

class HokuyoScipClient {
public:
  explicit HokuyoScipClient(const std::string& endpoint);
  bool start();
  bool stop();
  bool isRunning() const;
  bool fetch(RawScan& out);  // ノンブロッキング受信（将来）
};