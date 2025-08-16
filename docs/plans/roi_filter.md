# 点群範囲指定（ROI）実装計画

## 目的
- ROI で不要領域を除外し、誤検出/負荷を低減。

## 既存との整合
- `configs/default.yaml` に `world_mask.polygon` があり、ワールド座標の多角形を既に想定。

## 仕様
- include: 多角形/矩形（複数可）。exclude: 多角形/矩形（複数可）。
- センサー個別の `mask`（角度/距離）はローカル→ワールド変換前段で適用、ROIは統合後に適用。

## 実装
- `core/mask.h` に多角形包含判定（Even-Odd or winding）を追加。
- `core/transform.h` に基づき、`SensorManager` でローカル→ワールド変換→ROI適用→DBSCANへ。
- 高速化: AABB事前判定、リング（穴）対応、事前準備（辺法線/境界ボックスキャッシュ）。

## テスト
- 単体: 点包含/境界/穴。
- 統合: ROI on/off でクラスタ数や描画が期待通り変化する。
