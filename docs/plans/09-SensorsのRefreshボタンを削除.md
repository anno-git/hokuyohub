# 実装計画: Sensors の Refresh ボタンを削除

目的
- 接続直後にサーバ側から snapshot を自動配布しているため、手動の Refresh ボタンを廃止し、UIを簡素化する。
- センサー追加/削除などの操作後はサーバが broadcastSnapshot で全クライアントへ反映するため、ユーザー操作は不要。

対象
- ボタンDOMと関連JSの削除
  - [webui/index.html](webui/index.html)
  - [webui/app.js](webui/app.js)
- サーバ側の自動スナップショット
  - [LiveWs::handleNewConnection()](src/io/ws_handlers.cpp:10)

現状の把握
- 左ペイン Sensors のヘッダに Refresh ボタンがある。
  - DOM: <button id="btn-refresh">Refresh</button>（[webui/index.html](webui/index.html) 付近）
- クライアントは接続時および Refresh クリック時に snapshot 要求を送信している。
  - 接続時要求: ws.open で `sensor.requestSnapshot` を送信（[webui/app.js](webui/app.js:1152)）
  - Refresh クリック: `btnRefresh` の click で `sensor.requestSnapshot` と `filter.requestConfig` を送信（[webui/app.js](webui/app.js:1145)）
- サーバは新規WS接続時に sendSnapshotTo を自動実行するため、手動Refreshは冗長。
  - [LiveWs::handleNewConnection()](src/io/ws_handlers.cpp:10)

変更方針
- UIから Refresh ボタンを削除し、関連リスナ/コードを除去。
- snapshot の再同期が必要な箇所（センサーCRUDや設定ロード/インポート）では、サーバ側から broadcastSnapshot が既に行われているため機能に影響なし。
  - 例: センサー追加/削除の REST 成功後に [RestApi::postSensor()/deleteSensor()](src/io/rest_handlers.cpp) で `ws_->broadcastSnapshot()` 呼び出し済み。

具体手順
1) DOM からボタン削除
   - 対象: [webui/index.html](webui/index.html)
   - 削除要素: 
     - <button id="btn-refresh" type="button">Refresh</button>

2) JS から参照とリスナを削除
   - 対象: [webui/app.js](webui/app.js)
   - 変数参照削除: `const btnRefresh = document.getElementById('btn-refresh');`（[webui/app.js](webui/app.js:769)）
   - イベントリスナ削除: `btnRefresh?.addEventListener('click', ... )`（[webui/app.js](webui/app.js:1145)）
   - 以降のコードに btnRefresh 参照が無いことを確認

3) 接続時の初期要求の扱い
   - 現状、接続時にクライアントからも `sensor.requestSnapshot` を送っている（[webui/app.js](webui/app.js:1152)）。
   - サーバ側が接続直後に snapshot を push するため、二重になる可能性はあるが機能的には問題なし。
   - 任意最適化: 接続時の手動要求を削除し、サーバ主導のみとする（将来検討）。今回は安全のため現状維持でも可。

4) ドキュメント/UX
   - 操作ガイドやヘルプに「スナップショットは自動同期される」旨を追記（別タスク）。

受け入れ基準（テスト観点）
- Refresh ボタンがUIから消え、コンソールエラーが発生しない（btnRefresh 参照の残置が無い）。
- 起動/接続直後に Sensors 一覧が表示される（サーバからの snapshot により）。
- センサー追加/削除/編集による状態変更が自動で反映される（broadcastSnapshot 通知による）。
- 切断/再接続時も手動操作無しで最新状態が反映される。

ロールバック手順
- index.html に Refresh ボタンを復活。
- app.js に btnRefresh の参照と click リスナを復活。

作業粒度・変更規模
- 小（フロントエンド DOM/JS の削除のみ）。

関連ソース（参照）
- 接続時 snapshot push: [LiveWs::handleNewConnection()](src/io/ws_handlers.cpp:10)
- クライアント接続時要求: [ws.addEventListener('open', ...)](webui/app.js:1152)
- Refresh クリック送信: [btnRefresh click](webui/app.js:1145)
- センサー CRUD 後の broadcast: [RestApi::postSensor()](src/io/rest_handlers.cpp:281), [RestApi::deleteSensor()](src/io/rest_handlers.cpp:342)