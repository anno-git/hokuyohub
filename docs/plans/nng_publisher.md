# NNG パブリッシャ実装計画

## 目的
- クラスタ情報を外部へ配信（NNG pub/sub）。`configs/default.yaml` の `sinks` を参照。

## 現状と方針
- 既存: `io/nng_bus.{h,cpp}` が存在（BUSパターン想定）。
- 追加: 外部配信向けに `pub0` を用意し、`sinks[*].type == "nng"` を購読して起動。topic/encoding/rate_limit を解釈。

## メッセージ（MessagePack 案）
```json
{
  "v":1, "seq":123, "t_ns":1234567890,
  "items":[{"id":0,"cx":0.1,"cy":0.2,"minx":0.0,"miny":0.0,"maxx":0.2,"maxy":0.3,"n":42}],
  "raw": false
}
```
- UIの `clusters-lite` に整合（`app.js` が参照する `cx,cy,minx,..`）。

## 実装
- `io/nng_publisher.{h,cpp}` を新設。`start(cfg)`, `stop()`, `publish(frame/clusters)`。
- レート制御: `rate_limit` Hz 上限。超過時は最新のみ（古いフレーム破棄）。
- 再接続: 例外発生で指数バックオフ。

## テスト
- ループバック受信で MessagePack デコード確認。
- 高頻度(>60Hz)時のドロップ/レイテンシ計測。
