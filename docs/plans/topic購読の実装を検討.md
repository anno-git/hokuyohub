# 実装計画: topic 購読の実装を検討

目的
- Sink ごとに配信対象データ（トピック）を選択可能にし、外部連携の用途に応じて出力を最適化する。
- V1 は単一トピック選択（必須）で実装し、将来必要なら複数トピック/ワイルドカードへ拡張可能にする。

想定ユースケース
- NNG/OSC で
  - clusters のみ配信（軽量/高レベル）
  - raw/filtered の点群配信（可視化/分析）
  - world_mask（ROI）変更イベント配信（制御系）
  - 将来: sensor.status, performance.stats 等

既存の関連
- Sink モデルに `topic` フィールドが存在（保存/ロードに含まれる）。
  - 参照: Snapshot内 publishers.sinks の構築 [buildSnapshot()](src/io/ws_handlers.cpp:173)
  - REST の get/post/patch で topic を受け渡し済み（[src/io/rest_handlers.cpp](src/io/rest_handlers.cpp:690, 731, 850)）
- ランタイム出力は PublisherManager に委譲（現状は「設定反映」中心）: [applySinksRuntime()](src/io/rest_handlers.cpp:1440)

V1 設計方針（最小機能・拡張可能）
- トピック語彙（固定列挙）
  - "clusters": WSの clusters-lite と同等の構造を送出
  - "raw": WS の raw-lite と同等
  - "filtered": WS の filtered-lite と同等
  - "world": world.updated（world_mask変更）を送出
- Sink は 1 つの topic を選択。未設定なら "clusters" を既定。
- PublisherManager に pub-sub ルータを実装
  - 各イベント発生箇所から PublisherManager.publish(topic, payload) を呼び出す
  - 各 Sink は保持する topic に一致したメッセージだけ送出（rate_limit も適用）

対象
- ルーティング/送出:
  - [src/io/publisher_manager.cpp](src/io/publisher_manager.cpp), [src/io/publisher_manager.h](src/io/publisher_manager.h)
- イベント通知元（送出ポイント）
  - clusters: [LiveWs::pushClustersLite()](src/io/ws_handlers.cpp:117)
  - raw: [LiveWs::pushRawLite()](src/io/ws_handlers.cpp:130)
  - filtered: [LiveWs::pushFilteredLite()](src/io/ws_handlers.cpp:151)
  - world: [LiveWs::handleWorldUpdate()](src/io/ws_handlers.cpp:313) の成功ブロードキャスト箇所
- REST/UI
  - 現状の topic フィールドをそのまま使用
  - WebUI: Sinks の編集UIで topic セレクトを表示（既定のままでも良い）
    - 参照: [renderSinks()](webui/app.js:3004)

具体手順

1) PublisherManager にトピックルータを追加
- 構造:
  - SinkRuntime { type, url, encoding(nng), in_bundle/fragment(osc), topic, rate_limit, last_sent_time }
  - PublisherManager::configure(sinks):
    - SinkConfig の topic を読み取って SinkRuntime に保存
- API:
  - void publish(const std::string& topic, const Json::Value& payload)（JSON基準。NNG(msgpack)は内部でエンコード分岐）
  - publish 呼び出し時に全 SinkRuntime を走査し、topic 一致かつ rate_limit 許容のものだけ送信
- 送信形式:
  - nng: encoding により json or msgpack（計画 13 と連動）
  - osc: world や clusters を OSC メッセージにマッピング（初期は clusters/world のメタ情報中心でスキーマ最小）

2) 送出ポイントへの組み込み
- [LiveWs::pushClustersLite()](src/io/ws_handlers.cpp:117):
  - WSブロードキャストの直前/後に PublisherManager.publish("clusters", j) を呼ぶ
- [LiveWs::pushRawLite()](src/io/ws_handlers.cpp:130):
  - publish("raw", j)
- [LiveWs::pushFilteredLite()](src/io/ws_handlers.cpp:151):
  - publish("filtered", j)
- [LiveWs::handleWorldUpdate()](src/io/ws_handlers.cpp:313):
  - world_mask 反映後の broadcast と同時に publish("world", broadcast_msg)

3) UI（任意・V1推奨）
- Sinks 詳細UIに topic セレクトを組込み/整理
  - 対象: [renderSinks()](webui/app.js:3004)
  - セレクト肢: clusters, raw, filtered, world
  - PATCH /api/v1/sinks/:index で { "topic": "raw" } 等を送る（既存のPATCH経路でOK）

4) エラーハンドリング
- 未知トピック → configure 時に WARN を出して "clusters" にフォールバック or 該当 Sink を無効化
- 送出失敗（NNG/OSCエラー）→ WARN ログ、継続

5) 将来拡張（計画メモのみ）
- 複数トピック購読: topics: string[] を許可し、OR 条件で送出
- ワイルドカード: "raw.*" 等（実際は不要見込み）
- topic 別に encoding/出力スキーマを切替（高頻度トピックは MsgPack 固定など）

テスト計画
- 正常系:
  - Sink(topic=clusters) のみを設定 → clusters-lite だけが外部購読で受信
  - Sink(topic=raw/filtered/world) も同様に検証
- 複数 Sink:
  - clusters(osc), raw(nng-json), filtered(nng-msgpack) を同時送出 → 各受信者で整合確認
- 更新系:
  - PATCH で topic を切替 → 直後のフレームから出力対象が切替る
- エラーパス:
  - 不正 topic → WARN/フォールバックが記録され、送出は行わない or clusters に回避

ロールバック手順
- LiveWs 内の publish 呼出しをコメントアウト
- PublisherManager.publish で no-op に戻す

関連リンク
- WS push: [pushClustersLite()](src/io/ws_handlers.cpp:117), [pushRawLite()](src/io/ws_handlers.cpp:130), [pushFilteredLite()](src/io/ws_handlers.cpp:151)
- world update: [handleWorldUpdate()](src/io/ws_handlers.cpp:313)
- Sinks UI: [renderSinks()](webui/app.js:3004)
- ランタイム適用: [applySinksRuntime()](src/io/rest_handlers.cpp:1440)