# 04 REST API 実装方針（一覧と操作エンドポイント）

概要
- REST 側でセンサー一覧の取得と、必要最低限の操作（有効化/設定パッチ適用）を提供します。WebSocket 相当の機能を HTTP でも扱えるようにし、運用シナリオ（curl/スクリプト/他サービス連携）を支援します。

関連箇所
- REST コントローラ宣言: [RestApi](src/io/rest_handlers.h:10)
- 既存ハンドラ（未実装相当）: [RestApi::getSensors()](src/io/rest_handlers.cpp:4)
- センサー状態と更新: [SensorManager::listAsJson()](src/core/sensor_manager.cpp:394), [SensorManager::getAsJson()](src/core/sensor_manager.cpp:357), [SensorManager::setEnabled()](src/core/sensor_manager.cpp:328), [SensorManager::applyPatch()](src/core/sensor_manager.cpp:133)

提供エンドポイント（提案）
- GET /api/v1/sensors
  - 返却: センサー配列（listAsJson の生結果）
- POST /api/v1/sensors/{id}/enable
  - 入力: JSON { enabled: bool }
  - 動作: setEnabled(id, enabled) を実行。結果に応じて ok/error を返却。
- PATCH /api/v1/sensors/{id}
  - 入力: JSON patch（WS と同じスキーマ。例: pose/mask/endpoint/mode/skip_step/ignore_checksum_error）
  - 動作: applyPatch(id, patch, applied, err) を実行し、applied と最新 sensor JSON を返却。

方針
- AutoCreation は無効のため、[METHOD_LIST_BEGIN](src/io/rest_handlers.h:15) にメソッドを追加してルーティングを定義します。
- エラーハンドリングは HTTP ステータスコードで区別し、本文に type=error, message を含める（後方互換のため JSON 構造は WS と近似）。
- セキュリティ（認証/レート制限）は別タスクで扱い、本タスクでは入力の型/範囲バリデーションに注力。

実装ステップ
1. GET 実装
   - [RestApi::getSensors()](src/io/rest_handlers.cpp:4) を [SensorManager::listAsJson()](src/core/sensor_manager.cpp:394) で置換。
2. POST /enable 追加
   - メソッド追加（METHOD_LIST に追記）。
   - JSON から enabled の bool を抽出し、[setEnabled()](src/core/sensor_manager.cpp:328) を呼ぶ。
   - 成功: {type: "ok", ref: "sensor.enable"} と最新 [getAsJson()](src/core/sensor_manager.cpp:357) を返却。失敗: {type: "error", ...} を 400 等で返却。
3. PATCH /{id} 追加
   - メソッド追加。ボディ JSON を受け取り、[applyPatch()](src/core/sensor_manager.cpp:133) を呼ぶ。
   - 成功: {type: "ok", ref: "sensor.update", applied, sensor}。失敗: {type: "error", message}。
4. 入力検証（軽量版）
   - id の整数チェックと範囲チェック。
   - enabled の bool チェック。
   - patch は object 限定、存在するキーのみ処理（詳細強化はタスク 09）。

レスポンス設計
- 一貫性のため、WS と近いペイロードを返却する（type/ref/applied/sensor）。
- REST は HTTP ステータスも使用する（200/400/404/500）。

検証
- curl による動作確認（一覧/enable on/off/patch）。
- 不正入力で 4xx を返し、サーバが安定していること。

運用メモ
- 大量アクセスは想定しないため、初期は同期実装で十分。
- CORS 設定が必要なら drogon 側の設定で許可する（別タスク）。

完了条件
- GET /api/v1/sensors が正しい配列を返却。
- enable/patch が REST 経由でも成功し、WS と整合した状態に更新される。