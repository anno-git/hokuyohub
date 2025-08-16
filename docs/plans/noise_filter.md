# ノイズフィルタ実装計画

## 目的
- 前処理でノイズ点を除去し、クラスタ品質と安定性を改善。

## 方針
- `detect/dbscan.{h,cpp}` はクラスタ処理が中心。前段に `prefilter` ステージを新設する。
- 後処理 `postfilter` ステージも検討する。

## 手法
1. 近傍個数フィルタ（半径 r / 最小近傍 k）。距離に応じた r スケーリング。
2. 角度微分に基づくスパイク除去（|dR/dθ| > τ の跨ぎ点）。
3. 移動中央値/ロバスト回帰での外れ除去。
4. センサー強度/信頼度（対応するなら）による閾下除外。
5. 後処理では、クラスタ内の孤立点を除去してクラスタを再定義する方針。

## 実装
- `detect/prefilter.{h,cpp}` を新設し、`apply(points_in_world)` を提供。
- パラメタ: `prefilter: {"enabled": true, "k":5, "r_base":0.05, "dr_threshold":0.3, "median_window":5}` を YAML に追加。
- `SensorManager` のフレーム生成時に適用し、`clusters-lite`/NNG/OSC へはフィルタ後を渡す。

## 検証
- before/after の点数・クラスタ数・処理時間をロギングし最適域を探索。
- 既存UI（`app.js`）の raw 表示には**生点**も出せるよう、`raw-lite`は未フィルタ/オプション切替可能に。
