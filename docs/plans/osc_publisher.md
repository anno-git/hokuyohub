# OSC パブリッシャ実装計画

## 目的
- NNG と同等のクラスタ概要を OSC(UDP) で配信。

## 実装
- `io/osc_publisher.{h,cpp}` を新設。liblo もしくは軽量UDP整形。
- YAML: `sinks[*].type == "osc"` / `ip` / `port` / `path` / `rate_limit` を追加。

## メッセージ設計（案）
- アドレス: `/hokuyohub/cluster`
- 引数: `(i t i f f f f f f i)` = id, t_ns, seq, cx, cy, minx, miny, maxx, maxy, n
- 連続配信のため、フレーム内クラスタは複数メッセージに分割も可（MTU対策）。

## テスト
- libloの受信ツールで目視確認。
- 落ちてもbest-effortで継続。

