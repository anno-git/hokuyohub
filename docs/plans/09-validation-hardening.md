# 09 入力検証強化（applyPatch/WS ハンドラ）

概要
- センサー設定の動的更新と WS 経由の操作に対し、型・範囲・相関の検証を強化します。これにより不正入力によるクラッシュ、無用なデバイス再起動、異常状態の混入を防止します。エラー応答は一貫した形式に統一し、クライアント側の扱いを簡易化します。

関連箇所
- パッチ適用ロジック: [SensorManager::applyPatch()](src/core/sensor_manager.cpp:133)
- 有効/無効切替: [SensorManager::setEnabled()](src/core/sensor_manager.cpp:328)
- センサー状態出力: [SensorManager::getAsJson()](src/core/sensor_manager.cpp:357), [listAsJson()](src/core/sensor_manager.cpp:394)
- WS 更新ハンドラ: [LiveWs::handleSensorUpdate()](src/io/ws_handlers.cpp:119)
- WS 応答/ブロードキャスト: [sendSnapshotTo()](src/io/ws_handlers.cpp:91), [broadcastSensorUpdated()](src/io/ws_handlers.cpp:105)

目的
- 受理する JSON のキーと型を明確化（ホワイトリスト）。
- 値域（例: 角度、距離、skip_step、ignore_checksum_error）を検証。
- endpoint の正規化と安全なパース。
- エラー応答（type=error, ref, message）の統一。

入力仕様（パッチ JSON のホワイトリスト）
- enabled | on: bool
- pose: { tx: float, ty: float, theta_deg: float }
- mask: 
  - angle: { min_deg: float, max_deg: float }（[-180, 180]）
  - range: { near_m: float [>=0], far_m: float [>=0], near_m ≤ far_m）
- endpoint: 文字列 "host:port" または { host: string, port: int[0..65535] }
- mode: "MD" | "ME" | 将来拡張のため string（未知値は即時エラーとせず再起動要求を伴うかは運用方針次第）
- skip_step: int[≥1]
- ignore_checksum_error: 0 | 1

実装方針
- 検証ロジックを関数化
  - 例: `static bool validate_patch(const Json::Value& in, Json::Value& normalized, std::string& err)` を [applyPatch() 前段](src/core/sensor_manager.cpp:133) で使用。
  - normalized に内部表現へ正規化した値を格納（endpoint の分解、角度/距離の修正、near/far 入替など）。
- エラーの一貫性
  - エラー時は false とし、err に人間可読の短文（英語 or 日本語を決める。初期は英語で固定）を入れる。
  - WS 側は { type: "error", ref: "sensor.update", message: err }、REST 側は 400/422 を返す（REST は別ドキュメント参照）。
- 再起動要否の明確化
  - endpoint/mode/ignore_checksum_error/skip_step の変更はデバイス API によっては再起動が必要。ドライバが applyXXX を実装済みで true を返せる場合は再起動回避（[Hokuyo live reconfig 方針](docs/plans/07-hokuyo-live-reconfig.md)）。

詳細ステップ（applyPatch 側）
1) id の検証
   - 範囲: 0 ≤ id < slots.size()。不正なら "invalid id"。
2) JSON 形状と型の検証
   - オブジェクトであること。未知キーは無視またはエラー（初期は無視、将来は strict モードでエラー化）。
3) enabled/on
   - bool に強制変換不可の場合はエラー。
4) pose
   - tx, ty: 有限の float。theta_deg: 有限の float（[-360, 360] 程度にクランプしてもよい）。
5) mask
   - angle.min_deg/max_deg: [-180, 180] クランプ、min>max の場合は入替。
   - range.near_m/far_m: 0 以上、near>far の場合は入替。
6) endpoint
   - string or object。string は "host[:port]" を許容。port 数値変換に失敗したら 0 またはエラー。
   - host の長さと使用可能文字の簡易チェック。
7) mode
   - 空文字は不可。既知値の推奨（"MD"/"ME"）だが、未知値はデバイスが拒否する前提で上位エラーにするか、要件に応じて柔軟化。
8) skip_step
   - int 変換可、≥1。0 は無効（MVP では 1 以上を要求）。
9) ignore_checksum_error
   - 0 または 1 以外はエラー。
10) 反映と applied の構築
   - 正規化済みの値を cfg に反映し、applied に差分を構築。
   - 再起動要否フラグを集計し、必要なら [restartSensor()](src/core/sensor_manager.cpp:116) を呼ぶ（applyXXX が未実装/失敗の場合）。

WS 側の強化（handleSensorUpdate）
- 受領 JSON の検証（type, id, patch）の最低限チェック。
- applyPatch の戻り値に応じた応答。成功時は {type:"ok", ref:"sensor.update", applied, sensor} を返信し、[broadcastSensorUpdated()](src/io/ws_handlers.cpp:105) を呼ぶ。
- 失敗時は {type:"error", ref:"sensor.update", message} を返信。

ログ方針
- WARN: 無視した未知キー、境界にクリップした値。
- ERROR: 型不一致、範囲逸脱、endpoint パース失敗、再起動失敗、ドライバ applyXXX 失敗。

回帰防止（テスト連動）
- タスク 11 で validate_patch の単体テストを追加（正常/異常ケース、境界、相関 near/far 入替など）。
- WS 経由のエンドツーエンドも簡易に確認（モック SensorManager でも可）。

完了条件
- 明らかに不正な入力が状態を破壊しない。
- エラーメッセージが一貫し、クライアントから扱いやすい。
- 正常系は従来通り適用され、必要に応じて再起動/オンライン反映が行われる。