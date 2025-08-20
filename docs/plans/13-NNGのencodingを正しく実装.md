# 実装計画: NNG の encoding を正しく実装

目的
- Sink が nng のとき、設定された `encoding`（msgpack / json）に従って実際の送出フォーマットを切り替える。
- WebUI の選択（[renderSinks() の encoding セレクト](webui/app.js:3089)）→ REST 反映（[postSink/patchSink](src/io/rest_handlers.cpp:731)）→ ランタイム（PublisherManager 経由で NNG Publisher）まで一貫して適用。

対象
- ランタイム適用（PublisherManager）
  - 構成反映: [PublisherManager::configure()](src/io/rest_handlers.cpp:1441) から呼び出される管理クラス実装（[src/io/publisher_manager.cpp](src/io/publisher_manager.cpp), [src/io/publisher_manager.h](src/io/publisher_manager.h)）
- REST 受理部分（確認）
  - 追加: [postSink()](src/io/rest_handlers.cpp:731) で `encoding` を受理（msgpack/json）、[patchSink()](src/io/rest_handlers.cpp:850) で更新可
- WebUI（確認）
  - nng の追加/編集で encoding を選択・PATCH 送信（[webui/app.js](webui/app.js:3089, 3450)）

現状の把握
- REST 層は encoding を受け入れて AppConfig へ保存済み（[postSink](src/io/rest_handlers.cpp:795), [patchSink](src/io/rest_handlers.cpp:919)）。
- ランタイム適用は [applySinksRuntime()](src/io/rest_handlers.cpp:1440) → `publisher_manager_.configure(config_.sinks)` に委譲。
- PublisherManager 側で encoding を実際の出力へ反映しているかは未確認（本計画で実装/強化）。

変更方針
- PublisherManager に encoding の取り扱いを追加し、NNG Publisher インスタンスへ伝播。
- エンコード実装は 2 系統:
  - json: 既存 Json::Value / toStyledString() 等で文字列化し送出
  - msgpack: 依存導入または軽量実装（既存依存があればそれを利用）。なければ最初は JSON のみ実装し、msgpack はコンパイルフラグ/ライブラリ導入で段階追加。
- Sink ごとに topic / rate_limit / encoding を保持し、publish 時にフォーマット分岐。
- 出力対象データ:
  - clusters-lite, raw-lite, filtered-lite（WebSocket と同等構造）を NNG でも送出可能にする
  - topic に応じて payload を選択（topic 連携は別計画 14 で詳細設計）

具体手順

1) モデル定義/受け渡しの整理
- AppConfig の SinkConfig.nng().encoding（"msgpack"/"json"）を PublisherManager に渡す。
  - configure(const std::vector<SinkConfig>&) で Sink ごとの runtime 構造体（type, url, topic, rate_limit, encoding）を保持。
  - 参照: [applySinksRuntime()](src/io/rest_handlers.cpp:1441) 呼び出し元。

2) PublisherManager 実装拡張
- ファイル: [src/io/publisher_manager.cpp](src/io/publisher_manager.cpp), [src/io/publisher_manager.h](src/io/publisher_manager.h)
- 追加/変更:
  - ランタイムエントリに encoding フィールドを追加（列挙 or 文字列）。
  - NNG Publisher 生成時に encoding を注入。
  - publish(topic, data) API を用意し、内部で sink ごとに
    - topic マッチ判定（別計画 14）
    - rate_limit 判定
    - エンコード分岐（json/msgpack）
    - 実送信（nng_send）
    を実施。
- エラー処理:
  - 未サポート encoding の場合は構成時に無効化 or JSON にフォールバックして WARN ログ。

3) エンコーダ実装
- JSON:
  - clusters-lite/raw-lite/filtered-lite それぞれ、WS と同様の Json::Value 生成ロジック（[buildSnapshot()/push系ロジック](src/io/ws_handlers.cpp:117) を参考）で作成し、toStyledString() or toFastString() で std::string に。
- MessagePack:
  - 依存を導入可能なら msgpack-c 等を使用し、上記 JSON と同等フィールドをシリアライズ。
  - 導入が難しい場合、段階的実装として JSON のみ先行リリースし、msgpack は別PRで追加（この計画内に記載）。

4) データ注入ポイント
- 現在 WS は [LiveWs::pushClustersLite()](src/io/ws_handlers.cpp:117), [pushRawLite()](src/io/ws_handlers.cpp:130), [pushFilteredLite()](src/io/ws_handlers.cpp:151) で broadcast。
- 同タイミングで PublisherManager にも通知し、NNG/OSC に配信。
  - 例: pushClustersLite 内で PublisherManager.publish("clusters", json_payload) を呼ぶ（実装は別クラスに依存注入しておく）。
- PublisherManager への参照は LiveWs に注入済み or グローバル管理（現状に合わせる）。不足時は LiveWs に setter を追加。

5) WebUI/REST の整合確認
- 追加/編集/PATCH で encoding が正しく保持されることは [postSink](src/io/rest_handlers.cpp:795)/[patchSink](src/io/rest_handlers.cpp:919) で保証済み。
- snapshot（[buildSnapshot()](src/io/ws_handlers.cpp:173)）にも sinks 情報（encoding を含む）が乗っているため、UI 表示も維持。

6) ログ/メトリクス
- 構成適用時に encoding を INFO ログで出力。
- 送出失敗時（nng errno）を WARN/ERROR で記録。rate_limit 超過は DEBUG でスキップログ。

テスト計画
- 正常系:
  - nng(JSON) を設定 → 外部サブスクライバが JSON テキストを正しく受信。
  - nng(msgpack) を設定 → msgpack サブスクライバがデコード可能（依存導入後）。
  - sink 毎に encoding を異なる値にし、同時送信が正しく行われる。
- 設定変更:
  - PATCH で encoding を切替 → publisher reconfigure 後の新フォーマットで送出される。
- エラーパス:
  - 未サポート encoding を与える → 構成時にエラー（または JSON フォールバック）し、UI にエラーメッセージを返す（[patchSink](src/io/rest_handlers.cpp:919) は既にバリデーションあり）。

ロールバック手順
- PublisherManager の encoding 分岐を無効化し、JSON 固定へ戻す。
- LiveWs から PublisherManager.publish 呼び出しを一時無効化。

非機能
- 追加のエンコード処理に伴う CPU 負荷は軽微（フレームレートに影響しない範囲）。必要時にレート制限で制御。
- 依存導入（msgpack）はビルド設定の更新が必要。プラットフォームごとのリンクに注意。

関連リンク
- UI: [renderSinks() encoding セレクト](webui/app.js:3089)
- REST: [postSink() で encoding 受理](src/io/rest_handlers.cpp:795), [patchSink() で更新](src/io/rest_handlers.cpp:919)
- ランタイム入口: [applySinksRuntime() → PublisherManager::configure](src/io/rest_handlers.cpp:1441)
- WS送出（参考実装）: [pushClustersLite()](src/io/ws_handlers.cpp:117), [pushRawLite()](src/io/ws_handlers.cpp:130), [pushFilteredLite()](src/io/ws_handlers.cpp:151)