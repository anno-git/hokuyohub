# 10 Web UI 表示強化 方針（生点群表示/センサー別着色/接続指標）

概要
- 現在の UI はクラスタ簡易描画に特化しています。デバッグ効率向上と運用監視のため、以下を段階的に拡張します。
  - 生点群表示のトグル（クラスタと併用可）
  - センサー別の着色（凡例付き）
  - 接続状態/再接続/last seq age の指標表示

関連ファイル
- HTML: [webui/index.html](webui/index.html)
- JS: [webui/app.js](webui/app.js)
- CSS: [webui/styles.css](webui/styles.css)
- WS サーバ送信（受信メッセージ設計の参照先）: [LiveWs::pushClustersLite()](src/io/ws_handlers.cpp:78), [LiveWs::sendSnapshotTo()](src/io/ws_handlers.cpp:91)

現状整理
- クラスタの簡易描画は実装済み（円 + bbox）: [drawClusters](webui/app.js)
- センサーパネルとモーダル（設定編集）は実装済み: [webui/index.html](webui/index.html), [webui/app.js](webui/app.js)

拡張目標
1) 生点群描画（Raw Points）
   - サーバ側で集約済みの ScanFrame.xy/sid を UI に配信し、キャンバスへ点描画。
   - 大量点の描画負荷に留意し、WebGL への将来移行を見据える。
2) センサー別着色
   - sid ごとに固定色を割り当て、点群/クラスタの表示色に反映。
   - 凡例を UI に表示（sid → 色）。
3) 接続指標
   - ws の onopen/onclose/onerror のカウンタを保持し、stats 領域に表示。
   - last seq age = 現在時刻 - 最後に受信したフレーム時刻（t もしくはローカル受信時刻）を秒表示し、閾値超えで警告色へ。

クライアント側メッセージ設計
- 生点群を描画するには、WS で raw 点群を受け取る必要がある。
  - 方式A: 既存 clusters-lite に加えて、raw-lite（xy/sid の圧縮表現）を追加で送る。
  - 方式B: clusters-lite の拡張メッセージとして raw を任意フィールドで同梱する。
- 初期は方式Aを推奨（責務分離）。メッセージ例:
  - type: "raw-lite", t: uint64, seq: uint32, xy: Float32Array 風配列（JSON ではサイズ増大。MVP は JSON、将来はバイナリ/圧縮検討）
  - sid: Uint8Array 風配列（同上）
- サーバ側の送出は別タスク化してもよい（MVP では UI 内部でダミー描画に対応可能）。

UI 構成（提案）
- index.html の panel に UI トグルを追加:
  - 「Show Raw Points」チェックボックス
  - 「Per-sensor Colors」チェックボックス
  - 凡例エリア（センサー N 個に応じて動的生成）
- stats 表示の拡張:
  - 接続状態 connected/disconnected/error
  - reconnects 回数
  - last seq age（秒）
  - items（クラスタ数）に加えて points（点数、raw-lite 受信時）

描画ロジック（概要）
- 変換/スケーリングは既存のクラスタ描画と同一ロジックを再利用: [drawClusters](webui/app.js)
- 生点群:
  - xy をフラット配列としてループし、軽量な点描画（rect/lineTo でバッチ化）を行う
- 色付け:
  - sid→color は固定カラーパレット（HSL や定義済み配列）から割当
  - クラスタをセンサー別色にするか、点群のみ色分けかはトグルで選択可能（初期は点群のみ色分けで十分）

イベント/状態管理
- 追加の UI 要素に対して change リスナを登録し、状態フラグを保持（showRaw, perSensorColor）
- WS onmessage:
  - clusters-lite → 既存描画
  - raw-lite → 生点群バッファを更新し、フレーム同期で描画
- タイマ（setInterval）で last seq age の表示更新（毎秒など）

性能と劣化対策
- JSON の配列はサイズが大きくなるため、MVP では小容量で検証
  - 将来: サーバ側の raw-lite をバイナリ（ArrayBuffer）に変更（WS バイナリフレーム）
- キャンバス描画のバッチ化
  - 可能な限り beginPath/closePath 回数を削減
  - 1px 点は fillRect より path のほうが早い場合あり（環境依存）
  - ただし将来的にWebGLへの移行を前提としているので、簡易な対策に留める
- FPS の簡易計測を stats に追加（描画負荷の目視確認用）

アクセシビリティ/UX
- 凡例はスクロール可能（センサーが多い場合）
- 色覚多様性を考慮し、色数が増える場合は形状/透明度の差も検討

検証
- 生点群トグルの ON/OFF が即時反映
- センサー別着色が期待どおり（sid に応じて色が安定）
- 接続断の際、stats が正しく遷移（disconnected → reconnect → connected 等）
- last seq age が止まらずに更新され、しきい値超過で色が変わる

完了条件
- UI から生点群/色分け/接続指標を操作・確認できる
