# REST API 実装計画

## 目的
- ブラウザUIと同等の操作を HTTP 経由でも提供し、CIやCLIからも制御可能にする。
- 既存WS（`sensor.requestSnapshot`/`sensor.update`/`sensor.enable` ほか）と**payloadの一貫性**を保つ。

## スコープ
- センサー状態の取得・更新（enable/pose/mask/endpoint/mode 等）
- 設定ファイル load/import/export
- スナップショットの提供（WSの `sensor.snapshot` と同等内容）

## エンドポイント案
- `GET  /api/v1/sensors` → `[{"id", "name", "type", "endpoint", "enabled", "pose", "mode", "skip_step", "mask", ...}]`
- `GET  /api/v1/sensors/{id}` → 単体
- `POST /api/v1/sensors/{id}/enable` → `{"enabled": true}`
- `PATCH /api/v1/sensors/{id}` → `{"patch": {"pose":{"tx","ty","theta_deg"} , "endpoint":{"host","port"}, "mode", "skip_step", "mask":{"angle":{"min_deg","max_deg"}, "range":{"near_m","far_m"}}}}`
- `GET  /api/v1/snapshot` → `{"sensors":[...], "world_mask":..., "dbscan":..., "sinks":[...]}`

### 設定操作
- `POST /api/v1/configs/load`   → `{"name": "default.yaml"}`
- `POST /api/v1/configs/import` → multipart: `file=@*.yaml`
- `GET  /api/v1/configs/export` → `application/x-yaml`

## 実装詳細
- ルータ/実装: `io/rest_handlers.{h,cpp}` に統合。HTTPエンジンは既存のサーバに合わせる（Drogon/独自）。
- 内部適用は `core/sensor_manager` 経由のスレッドセーフAPIで行う。
- `configs/default.yaml` の構造（`world_mask.polygon`, `dbscan.*`, `sinks[*]`）をそのままJSONに射影。
- **WS一貫性**: UIは `app.js` で `sensor.update`, `sensor.enable`, `sensor.requestSnapshot` を送信、`sensor.snapshot`, `sensor.updated`, `clusters-lite`, `raw-lite` を受信。RESTでも同キー名/同形状を踏襲。

## セキュリティ
- `Authorization: Bearer <HOKUYO_API_TOKEN>` の静的トークン検証（env または YAML）。
- CORSはローカルUI想定の緩め設定だが、originsの許可リストを用意。

## エラーレスポンス
- 400/404/409/500 を使用し、`{"type":"error", "message", "code", "request_id"}` 形式で返却。

## テスト
- curlスモーク（enable/patch/snapshot/export/import）。
- UIと併用時の競合・再入保証（idempotent POST、PATCHの部分更新）。
