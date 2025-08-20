# 実装計画: Filter の Reset to Default ボタンを削除

目的
- フィルタ既定値の単一ソースをサーバ側（AppConfig/FilterManager）に集約し、UI上の「Reset to Default」ボタンを廃止して一貫性を高める。
- 既定状態への復帰は Configs Load（例: default.yaml をロード）で代替。

対象
- ボタンDOM/JSの削除
  - [webui/index.html](webui/index.html:163)
  - [webui/app.js](webui/app.js:1423)
- サーバ（参照のみ）
  - 既定値配布: [sendFilterConfigTo()](src/io/ws_handlers.cpp:472)
  - 反映/配信: [handleFilterUpdate()](src/io/ws_handlers.cpp:425), [broadcastFilterConfigUpdate()](src/io/ws_handlers.cpp:457)
  - 設定ロード/インポート: [postConfigsLoad()](src/io/rest_handlers.cpp:1144), [postConfigsImport()](src/io/rest_handlers.cpp:1256)

現状の把握
- 右ペイン Filter セクションに Reset ボタンがある（id=btn-reset-filters）。
  - DOM: [webui/index.html](webui/index.html:163)
- JS 側で要素参照とイベント登録がある。
  - 要素参照: filterElements.resetFiltersBtn（[webui/app.js](webui/app.js:1423)）
  - クリックで resetFilterConfig() 実行（[webui/app.js](webui/app.js:1654)）
  - resetFilterConfig() はクライアント既定値を適用のうえ sendFilterConfig() 送信（[webui/app.js](webui/app.js:1572)）

変更方針
- Reset ボタンの DOM を削除。
- app.js の resetFilterConfig() と関連イベントの削除。
- クライアントの暫定既定 currentFilterConfig（[webui/app.js](webui/app.js:1429)）は「接続直後にサーバの `filter.config` で上書きされる」ため残置可。ただしコメントで「暫定値」明記。
- 既定に戻す運用は Configs Load（例: default.yaml をロード）を案内。

具体手順
1) index.html からボタン削除
   - 対象: [webui/index.html](webui/index.html:163)
   - 削除要素: <button id="btn-reset-filters" type="button">Reset to Default</button>

2) app.js の関連コード削除
   - 対象: [webui/app.js](webui/app.js)
   - 削除:
     - filterElements.resetFiltersBtn の参照（[webui/app.js](webui/app.js:1423)）
     - setupFilterEventListeners() 内の click 登録（[webui/app.js](webui/app.js:1654)）
     - resetFilterConfig() 本体（[webui/app.js](webui/app.js:1572)）
   - コメント追加:
     - currentFilterConfig は接続直後の `filter.config` により上書きされる旨を [webui/app.js](webui/app.js:1429) 付近に記載。

3) ドキュメント/ヘルプ
   - 「既定に戻す」= default.yaml を Configs Load でロードする手順を docs に追記（別タスクでも可）。

受け入れ基準（テスト観点）
- UI から Reset ボタンが消え、JS エラーが発生しない（未参照の要素/関数が残っていない）。
- 接続直後にサーバ配布の `filter.config` が UI に反映される。
- default.yaml を Load すると、UI が既定値へ戻る。

ロールバック手順
- index.html にボタンを復活。
- app.js に resetFilterConfig() とイベントを復活。

作業粒度・変更規模
- 小（フロントエンド DOM/JS の削除のみ）。

関連リンク
- 既定値配布: [sendFilterConfigTo()](src/io/ws_handlers.cpp:472)
- 変更反映: [handleFilterUpdate()](src/io/ws_handlers.cpp:425)
- 配信: [broadcastFilterConfigUpdate()](src/io/ws_handlers.cpp:457)
- 設定ロード: [postConfigsLoad()](src/io/rest_handlers.cpp:1144)