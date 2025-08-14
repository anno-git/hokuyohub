# 08 サンプル設定ファイル configs/default.yaml 作成方針

概要
- すぐに起動検証できる最小構成の YAML を提供します。既存のローダが解釈するキー/構造に厳密に合わせ、コメントで意味を明示します。

関連箇所
- 設定ロード: [load_app_config()](src/config/config.cpp:7)
  - センサー配列の読み込み: [sensors 節](src/config/config.cpp:11)
  - endpoint の host:port 分解: [処理箇所](src/config/config.cpp:18)
  - enabled/mode/interval/skip_step: [読み込み](src/config/config.cpp:29)
  - ignore_checkSumError → ignore_checksum_error へのマッピング: [読み込み](src/config/config.cpp:35)
  - pose/mask の正規化: [pose](src/config/config.cpp:42), [mask/angle](src/config/config.cpp:48), [mask/range](src/config/config.cpp:57)
- 既定パスと CLI オプション: [main の cfgPath 初期値/--config](src/main.cpp:12)

目的
- 最低限 1 台の Hokuyo URG ETH を前提とした例を提示し、UI/WS/REST が起動・可視化できる状態にする。
- パラメータ名/階層/スカラー型をローダ実装と一致させる。

YAML 構成（方針）
- トップレベル:
  - sensors: 配列（1件〜複数）
  - dbscan: { eps, minPts }（MVP では run 実装後に有効）
  - ui: { ws_listen, rest_listen }（どちらも drogon の addListener に利用）
  - sinks: 配列（type=nng の url を指定すると main で優先適用される: [sink 適用](src/main.cpp:25)）
- sensors[i]:
  - id: 文字列（例: "urg-1"）
  - type: "hokuyo_urg_eth"（ドライバ工場のキー: [create_sensor()](src/sensors/SensorFactory.cpp:4)）
  - name: 任意
  - endpoint: "HOST:PORT" 形式（host と port に分解される: [処理箇所](src/config/config.cpp:20)）
  - enabled: bool
  - mode: "ME" or "MD"（強度あり/なし: [toUrgMode()](src/sensors/hokuyo/HokuyoSensorUrg.cpp:84)）
  - interval: 測定間隔 ms（0 は既定）
  - skip_step: 0 以上の整数（ダウンサンプリング）
  - ignore_checkSumError: 0/1（ローダ側で ignore_checksum_error へ変換: [読み込み](src/config/config.cpp:35)）
  - pose: { tx, ty, theta }（theta は度）
  - mask:
    - angle: { min, max }（度、ローダで正規化・クランプ: [処理](src/config/config.cpp:54)）
    - range: { near, far }（メートル、near/far の入れ替え正規化: [処理](src/config/config.cpp:60)）

作成指針
- 実ネットワークが未接続でも UI を確認できるよう、enabled=false の例と、スタブ動作確認用に enabled=true の例を併記。
- URG の代表的なポート 10940 をコメントで示す。
- dbscan は初期値（eps≈0.12, minPts≈6）を推奨として記載。
- ui の listen は "0.0.0.0:8080" を既定例にする（LAN 内からの確認用）。
- sinks は空配列または nng 1件（後で有効化できるようコメント化）。

バリデーション観点（ローダ依存）
- angle[min,max] は [-180,180] にクランプ: [処理](src/config/config.cpp:54)。
- range[near,far] は 0 以上で near>far の場合は入替: [処理](src/config/config.cpp:60)。
- skip_step は 0 以上: [読み込み](src/config/config.cpp:33)。

サンプルの雛形（説明コメント付き）
- 担当者は以下の雛形を configs/default.yaml として配置。値は自環境に合わせて編集。

設置/起動手順（運用メモ）
1) ファイル配置: configs/default.yaml を作成。
2) 実行: バイナリに `--config ./configs/default.yaml` を渡す（未指定時は [既定パス](src/main.cpp:12) を参照）。
3) ブラウザで http://HOST:PORT/ にアクセス（[DocumentRoot](src/main.cpp:48) は webui）。

検証
- 起動ログにセンサー設定件数と HTTP リスナのバインドが表示される。
- WS 接続成功（stats=connected）と、センサーパネルの snapshot が表示される。
- URG 実機接続時はセンサーが started ログを出し、クラスタ配信（DBSCAN 統合後）を確認。

運用上の注意
- 実機ネットワークではセンサー側の IP とサブネットが一致していることを確認。
- sinks の nng を公開ネットワークに露出させない（内部向けセグメントで運用）。

将来拡張
- 複数センサーの相対 Pose 設定例の追加。
- 環境別プリセット（dev/staging/prod）とオーバーレイ構成管理。