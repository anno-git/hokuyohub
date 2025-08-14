# 11 テスト計画（単体/結合/性能）

概要
- 重要ロジックの回帰を防ぎ、最適化や置換を安全に行うためのテスト方針を示します。フレーム処理系はリアルタイム性を考慮し、単体テストでロジックを切り出し、結合テストでは軽量モックを用いて I/O 依存を排除します。

対象範囲
- マスク/座標変換/設定ロード/クラスタリング/マネージャのパッチ適用/WSハンドラの最小バリデーション。
- 性能の目安計測（デバッグ時限定）。

フレームワーク/レイアウト（推奨）
- C++: GoogleTest or Catch2 を想定（どちらでも可）。テストコードは tests/ 以下に配置。
- データファイル（YAML サンプル等）は tests/data/ に配置。
- 複雑な I/O を伴う箇所はインタフェース注入またはモックで代替。

参照（主な対象ソース）
- マスク判定: [`pass_local_mask()`](src/core/mask.h:6)
- 変換: [`apply_pose()`](src/core/transform.h:4)
- 設定読込: [`load_app_config()`](src/config/config.cpp:7)
- DBSCAN 本体: [`DBSCAN2D::run()`](src/detect/dbscan.cpp:4)
- センサ管理:
  - 設定反映: [`SensorManager::applyPatch()`](src/core/sensor_manager.cpp:133)
  - 有効/無効: [`SensorManager::setEnabled()`](src/core/sensor_manager.cpp:328)
  - 再起動: [`SensorManager::restartSensor()`](src/core/sensor_manager.cpp:116)
  - 状態出力: [`SensorManager::getAsJson()`](src/core/sensor_manager.cpp:357), [`SensorManager::listAsJson()`](src/core/sensor_manager.cpp:394)
- WS ハンドラ（最低限の入力検証と応答フォーマット）:
  - スナップショット送信: [`LiveWs::sendSnapshotTo()`](src/io/ws_handlers.cpp:91)
  - 更新処理: [`LiveWs::handleSensorUpdate()`](src/io/ws_handlers.cpp:119)
  - ブロードキャスト: [`LiveWs::broadcastSensorUpdated()`](src/io/ws_handlers.cpp:105)

単体テスト方針

1) マスク判定（角度単位修正後）
- 正常系
  - min_deg=0, max_deg=10 のとき、5° が通過し -5°/15° が除外される。
  - min_deg=-10, max_deg=10 の境界（±10°）の包含方針を固定し検証。
- 異常/境界
  - 設定で min>max が来てもロード側で正規化される前提（このケースは load_app_config で検証）。
- 参照: [`pass_local_mask()`](src/core/mask.h:6)

2) 座標変換
- 回転/並進の組み合わせで既知値と一致するか（0°, 90°, 180°, 270°、および非直交角）。
- 浮動小数の許容誤差を明確化（例: 1e-5）。
- 参照: [`apply_pose()`](src/core/transform.h:4)

3) 設定ロード
- sensors/ui/dbscan/sinks の各キーがデフォルトを保ちつつ上書きされるか。
- endpoint の "host:port" 分解、port 省略時の挙動。
- mask/pose の正規化（角度クランプ、near/far 入替）。
- 参照: [`load_app_config()`](src/config/config.cpp:7)

4) DBSCAN2D
- 正当性
  - 二つの明確なクラスタ＋ノイズでクラスタ数・count・bbox・centroid 一致。
  - eps と minPts の境界ケース（密度が一つ下でノイズになる）。
- センサー混在
  - sid を混在させ、sensor_mask ビット和が期待通り。
- 性能（デバッグ計測）
  - 代表点数（例: 3k, 10k）で平均/最大実行時間が目標内（例: 10ms 以下）であること。
- 参照: [`DBSCAN2D::run()`](src/detect/dbscan.cpp:4)

5) SensorManager::applyPatch
- ホワイトリスト項目ごとに正常更新され applied に反映される。
  - enabled/on、pose（flat/nested）、mask、endpoint（string/object）、mode、skip_step、ignore_checksum_error。
- 範囲/型チェック（不正入力で false とエラーメッセージ）。
- 再起動フラグの動作（applyXXX が false/未実装の場合に restartSensor が呼ばれる）。
- state 出力との整合（更新後の [`getAsJson()`](src/core/sensor_manager.cpp:357) が applied と整合）。

6) SensorManager::setEnabled / restartSensor
- start 未済のスロットに対し enable→start が呼ばれる（モックドライバで確認）。
- disable で stop が呼ばれ started=false になる。
- restartSensor が start→started=true/false を正しく反映。

7) WS ハンドラ最小検証
- sensor.update に対して、成功時 {type:"ok", ref:"sensor.update", applied, sensor} が返る。
- 失敗時 {type:"error", ref:"sensor.update", message} が返る。
- sensor.requestSnapshot で {type:"sensor.snapshot", sensors:[...]} が返る（モック SensorManager で値固定）。

モック戦略
- ISensor モック
  - start/stop/subscribe をスタブし、start/stop の呼び出し回数をカウントできるようにする。
  - applyMode/applySkipStep は設定で true/false を返すモードに切替可能にして、再起動の分岐をテスト。
- SensorManager 周辺
  - slots 内 dev をモックで差し替える（ファクトリを注入する or テスト専用の create_sensor を差し替える戦略）。
- WS 側
  - WebSocket 接続オブジェクトは drogon 抽象のモック/フェイクで send の引数を捕捉。

テストデータ（例）
- YAML（tests/data/）:
  - default-minimal.yaml: 最小1センサー、デフォルト優先。
  - masks-edge.yaml: 角度/距離の境界ケース。
  - endpoint-variants.yaml: "host:port" とオブジェクト形式の混在。
- 点群（DBSCAN 用）:
  - grid-2clusters.json: 二塊＋ノイズ。
  - dense-border.json: eps 境界のデータ群。

ビルド/実行（例）
- CMake にテストターゲットを追加し、CI で実行（フレームワークは任意）。
- ローカルでは `ctest -j` 等で並列実行。
- ベンチ系は OFF がデフォルト、環境変数/タグで opt-in 実行。

性能監視（任意）
- DBSCAN の処理時間を計測し、しきい値を超えた場合に WARN ログ（テスト環境のみ）。
- アロケーション回数（optional・高度）を可視化して回帰検出。

保守基準/完了条件
- 主要関数の単体テストが揃い、正常/異常/境界が網羅される。
- センサーパッチ適用と WS 応答の往復がテストで担保される。
- DBSCAN の正当性と目安性能が確認される（CI で安定実行）。

将来拡張
- センサー複数台のポーズ整合性検証（混合点群の幾何条件チェック）。
- NNG 送信のワイヤフォーマット検証（送受信の相互テスト）。
- SensorManager::stop（停止処理）結合テスト（スレッド終了/リソース解放）。
  - 参照: [`SensorManager::start()`](src/core/sensor_manager.cpp:230) と停止方針ドキュメント（docs/plans/06-sensor-manager-stop.md）