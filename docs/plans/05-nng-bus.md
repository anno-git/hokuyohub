# 05 NNG パブリッシャ実装方針

概要
- クラスタ結果を外部プロセスへ配信するためのパブリッシュ機構を実装します。ビルドフラグ USE_NNG により有効化し、MessagePack 等のバイナリ形式で配信します。初期はシンプルな pub-only で開始し、将来的にトピック/レート制御を拡張します。

関連箇所
- スタブ実装: [NngBus::startPublisher()](src/io/nng_bus.cpp:8), [NngBus::publishClusters()](src/io/nng_bus.cpp:9)
- ビルドガード: [USE_NNG マクロ参照](src/io/nng_bus.cpp:2)
- 起動箇所: [main での起動と URL 決定](src/main.cpp:25), [パブリッシャ開始](src/main.cpp:33)
- 型: [Cluster 構造体](src/detect/dbscan.h:7)

目的
- tcp://host:port 等の URL でクラスタを pub 配信できる状態にする。
- USE_NNG 無効時でもアプリ全体は動作継続（NO-OP）。

プロトコル/ペイロード設計（初期）
- 1メッセージ=1フレームのクラスタ群。
- フィールド案
  - t: uint64（ns）
  - seq: uint32
  - items: 配列（各 item は id: u32, sensor_mask: u8, cx, cy, minx, miny, maxx, maxy: float32, count: u16）
- バイナリ形式: MessagePack を想定（軽量で多言語対応が容易）。
- トピック: 初期は未使用。将来は SinkConfig.topic をヘッダや先頭プレフィックスとして付与（互換性維持のため別メッセージ種別にするのも可）。

実装方針
- startPublisher
  - USE_NNG 有効時のみ pub0 ソケットを生成。
  - URL が tcp://0.0.0.0:PORT の場合は listen、tcp://HOST:PORT の場合は dial を選択する方針（シンプルさ重視でどちらかに固定でも可）。
  - url_ を保存し、エラー時はログ出力してフォールバック（publish は NO-OP）。
- publishClusters
  - USE_NNG 有効時のみシリアライズ→送信。
  - バッファは再利用（静的に std::vector<uint8_t> をワーク領域として確保/拡張）。
  - 送信失敗時はログに集約（フレーム落ち容認、アプリは継続）。
- USE_NNG 無効
  - いずれも NO-OP とし、ビルド依存を排除。

エラーハンドリング
- ソケット生成/バインド/ダイヤル失敗: ログに理由を出力し、以後 publish はスキップ（NO-OP）。
- 送信失敗: ログで件数集計（一定間隔でまとめて警告）。連続失敗で再初期化は将来タスク。
- 異常ペイロード: 発生しない前提（型が固定）だが、シリアライズ例外はキャッチしてスキップ。

性能/メモリ
- 1フレームにつき items 数に比例したメモリコピーが発生。バッファ再利用で再確保を回避。
- 送信はノンブロッキング前提（nng_send のデフォルト動作を確認）。ブロック懸念がある場合は送信ワーカースレッド化を将来検討。

ビルド/リンク
- ビルドオプションで USE_NNG を定義。
- nng ライブラリとヘッダへの依存を CMake 側に追加（本タスクでは方針のみ）。
- 無効時でもコンパイルエラーとならないように #ifdef で囲う: [USE_NNG マクロ参照](src/io/nng_bus.cpp:2)

API/設定との連携
- URL は設定から優先的に適用: [sinks スキャンと pubUrl 反映](src/main.cpp:25)
- rate_limit, topic, encoding は将来拡張（初期は無視 or ログに表示）。

検証
- nng のサンプルサブスクライバ（別プロセス）で受信確認。
- 異常 URL/ポート占有/ネットワーク未接続時の挙動（アプリ継続/ログのみ）を確認。
- 大きな items 配列でも連続送出でフリーズしないこと。

後方互換/拡張
- エンベロープに version フィールドを追加可能な余地を残す（将来の schema 変更に備える）。
- topic が必要になった場合はヘッダ部 or nng のトピック機能のどちらかを採用（要調査）。

セキュリティ/運用
- 初期は平文・無認証で可。ネットワーク範囲を局所セグメントに限定すること。
- 公開ネットワーク越しの利用が必要になった場合、暗号化や認証は別レイヤ（例えば curve25519 を使う NNG 拡張やトンネル）で対応。

実装手順（推奨）
1. USE_NNG 有効時のコンパイル体制を整える（CMake 等）。
2. [NngBus::startPublisher()](src/io/nng_bus.cpp:8) にソケット生成/URL 設定/エラーハンドリングを実装。
3. [NngBus::publishClusters()](src/io/nng_bus.cpp:9) にメッセージのシリアライズと送信を実装。
4. main の起動ログに URL と USE_NNG の有無を明示: [main での開始ログ付近](src/main.cpp:56)
5. 別プロセスの SUB で受信テスト。負荷テスト（連続1分送出）を実施。

完了条件
- USE_NNG 有効時にクラスタが外部プロセスで受信できる。
- USE_NNG 無効時でもアプリが正常に動作し続ける（ログに NO-OP 旨を出す程度）。