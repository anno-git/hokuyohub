# HokuyoHub 実装方針集

この文書は、現在のTODO一覧の各項目について実装の方針を示します。具体実装は担当者に委ねます。必要に応じてソース参照はクリック可能な形で示します。

目次
- 01. 角度単位バグ修正（マスク判定）
- 02. mainのダミークラスタをDBSCAN出力に置換
- 03. DBSCAN2D 実装方針
- 04. REST API getSensors と操作エンドポイント
- 05. NNGパブリッシャ実装
- 06. SensorManager の停止処理
- 07. Hokuyo ドライバの動的設定反映
- 08. サンプル設定ファイル configs/default.yaml
- 09. パッチ適用とWSの入力検証強化
- 10. Web UI の表示強化
- 11. テスト計画

---

01. 角度単位バグ修正（マスク判定）

背景
- 集約ループでは角度をラジアンで計算しています: [step_to_angle_rad() 使用箇所](src/core/sensor_manager.cpp:287) と [apply_pose() 適用](src/core/sensor_manager.cpp:293)。
- 一方、マスクしきい値は度数法で保持されています: [SensorMaskLocal](src/config/config.h) と判定関数 [pass_local_mask()](src/core/mask.h:6)。
- 現状はラジアン値を度しきい値と直接比較しており不正です。

目的
- ローカルマスクの角度比較を単位整合させ、意図したスキャン点の抑制が行われる状態にする。

影響範囲
- 判定関数 [pass_local_mask()](src/core/mask.h:6)。
- 集約ループの角度生成 [SensorManager 集約ループ](src/core/sensor_manager.cpp:283)。

方針
- 比較の直前で「角度を度へ変換」するか「しきい値をラジアンへ変換」する方針のいずれかに統一する。
- 可読性の観点から、判定関数内部で角度ラジアン→度へ変換して比較するのが明確。
- 近接距離しきい値はメートルで既に整合済みのため変更不要。

実装ステップ
- [pass_local_mask()](src/core/mask.h:6) 内で angle を度に変換して比較する。
- ユニットテストを用意し、閾値境界 ±180 度などの端点を含めて確認する。

検証
- マスク角を 0〜10 度等に設定し、該当ステップのみ通過することを可視化で確認。
- 異常値（min>max）については設定ロード側で正規化済み: [load_app_config()](src/config/config.cpp:48)。

完了条件
- 既知の設定で予想どおりの点が除外される。
- テストがグリーン。

---

02. main のダミークラスタを DBSCAN 出力に置換

背景
- 現状の送出は固定ダミーデータ: [ダミークラスタ送出箇所](src/main.cpp:68)。

目的
- 集約点群に対して [DBSCAN2D::run()](src/detect/dbscan.cpp:4) を実行し、その結果をWSとNNGへ配信する。

影響範囲
- エントリポイント [main()](src/main.cpp:11) のコールバック。
- バス配信 [NngBus::publishClusters()](src/io/nng_bus.cpp:9) とWS [LiveWs::pushClustersLite()](src/io/ws_handlers.cpp:78)。

方針
- SensorManager のコールバックで受けた ScanFrame の xy と sid をそのまま DBSCAN に渡す。
- 結果の Cluster を既存WSメッセージ形式に整形して送信。NNGも同一構造のシリアライズを使用。

実装ステップ
- [main() のコールバック](src/main.cpp:68) にて DBSCAN を呼び出し、戻り値を WS/NNG へ。
- パラメータは設定から生成済み: [DBSCAN2D インスタンス化](src/main.cpp:38)。

検証
- 点群を人工的に生成し、クラスタ数やバウンディングボックスが期待に一致すること。

完了条件
- ダミーデータが完全に排除され、実クラスタのみがフロントに表示される。

---

03. DBSCAN2D 実装方針

背景
- 実装本体はスタブ: [DBSCAN2D::run()](src/detect/dbscan.cpp:4)。

目的
- 30 FPS 前後で期待点数を処理できる 2D DBSCAN を提供する。

影響範囲
- [dbscan.cpp](src/detect/dbscan.cpp) と [dbscan.h](src/detect/dbscan.h)。

方針
- 近傍探索を高速化するためにグリッド分割（セルサイズ≈eps）を用いる。
- xy は [x0,y0,x1,y1,...] のフラット配列で渡されるため、インデクシングは点インデックス i に対し x=xy[2i], y=xy[2i+1]。
- sid はクラスタの sensor_mask 集計に使用するだけなので近傍半径判定には不要。
- クラスタID採番と訪問フラグを用い、典型的なDBSCANの拡張方式で実装。

実装ステップ
- 前処理: 空間グリッド構築（セル座標→点インデックスのマルチマップ）。
- 各点について未訪問なら近傍探索→コア点判定→クラスタ拡張。
- Cluster の bbox と重心、count、sensor_mask を逐次更新。
- メモリを極力使い回し、オブジェクトの再確保を抑制。

検証
- 少数ケースで手計算と一致。
- ノイズ点、境界条件、複数センサー混在の bitmask が正しく出る。
- 性能: 代表点数で1フレーム当たり目標時間以内（例: <10ms）。

完了条件
- 実運用負荷でフレーム落ちがなく、WS表示が滑らか。

---

04. REST API getSensors と操作エンドポイント

背景
- REST は雛形のみ: [RestApi::getSensors()](src/io/rest_handlers.cpp:4) は空配列。

目的
- センサー一覧を返し、必要に応じて enable/patch 相当の操作をHTTPでも提供する。

影響範囲
- [RestApi](src/io/rest_handlers.h) と [rest_handlers.cpp](src/io/rest_handlers.cpp)。
- [SensorManager::listAsJson()](src/core/sensor_manager.cpp:394), [SensorManager::applyPatch()](src/core/sensor_manager.cpp:133), [SensorManager::setEnabled()](src/core/sensor_manager.cpp:328)。

方針
- GET /api/v1/sensors は [listAsJson()](src/core/sensor_manager.cpp:394) をそのまま返却。
- POST /api/v1/sensors/{id}/enable で on/off を切替。
- PATCH /api/v1/sensors/{id} で JSON パッチを適用（WS と同じ項目）。
- 認証やレート制限は別タスクで最小限の検証のみ行う。

実装ステップ
- [RestApi::getSensors()](src/io/rest_handlers.cpp:4) を実装し JSON を返却。
- 必要ならルーティングを追加（AutoCreation無効につき [METHOD_LIST_BEGIN](src/io/rest_handlers.h:15) を拡張）。

検証
- curl で一覧と操作を確認。WS と同様の結果が得られること。

完了条件
- 少なくとも一覧が正しく取得でき、将来の操作API拡張の足場がある。

---

05. NNG パブリッシャ実装

背景
- 現在はスタブ: [NngBus::startPublisher()](src/io/nng_bus.cpp:8), [NngBus::publishClusters()](src/io/nng_bus.cpp:9)。

目的
- クラスタ情報を外部へ配信可能にする（例: tcp://host:port）。

影響範囲
- [nng_bus.cpp](src/io/nng_bus.cpp) とビルドフラグ USE_NNG。
- エントリポイントからの起動: [main() 内 pub 初期化](src/main.cpp:33) と URL 選択 [sink 適用箇所](src/main.cpp:25)。

方針
- USE_NNG 有効時のみ nng_pub0 ソケットを生成し listen ないし dial。
- シリアライズは軽量な MessagePack を想定し、最低限のスキーマに固定。
- 送出頻度はフレーム毎で開始し、将来的なレート制限は SinkConfig.rate_limit を考慮。

実装ステップ
- startPublisher でソケット生成と URL 設定、終了時クリーンアップはプロセス終了に委ねる。
- publishClusters で buffer 構築→送信。USE_NNG 無効時は NO-OP。

検証
- nng_sub クライアントで受信できること。

完了条件
- ビルドフラグ有無で挙動が切り替わり、クラスタが他プロセスから購読可能。

---

06. SensorManager の停止処理

背景
- 停止/合流が未実装: [備考コメント](src/core/sensor_manager.cpp:321)。

目的
- プロセス終了時や将来のリロードに備え、スレッドとデバイスを安全に停止させる。

影響範囲
- [SensorManager::start()](src/core/sensor_manager.cpp:230) のスレッド管理。
- ISensor 実装の stop 呼び出し。

方針
- running フラグを原子更新し、sleep_until ループを抜けて join。
- join 後に全スロットの dev->stop() を保証する。

実装ステップ
- SensorManager に stop() を追加し、State.running=false、thread.join() を実装。
- main の終了フローで stop() を呼ぶフックを追加（例: atexit ハンドラなど）。 

検証
- 連続起動/停止テストでリソースリークやゾンビスレッドが無いこと。

完了条件
- 複数回の起動停止に耐え、デバイスが確実に閉じる。

---

07. Hokuyo ドライバの動的設定反映

背景
- ランタイム変更は再起動前提。ISensor にはフックあり: [applySkipStep()](src/sensors/ISensor.h:26), [applyMode()](src/sensors/ISensor.h:27)。
- 実装クラスには未定義: [HokuyoSensorUrg](src/sensors/hokuyo/HokuyoSensorUrg.h)。

目的
- 可能な範囲で測定中に設定反映し、データ断を最小化する。

影響範囲
- [HokuyoSensorUrg.h](src/sensors/hokuyo/HokuyoSensorUrg.h:17), [HokuyoSensorUrg.cpp](src/sensors/hokuyo/HokuyoSensorUrg.cpp:90)。

方針
- デバイスAPIが許す場合、測定停止→パラメタ適用→測定再開を短時間で行う。
- 設備上不可の場合は false を返し、[SensorManager::applyPatch()](src/core/sensor_manager.cpp:133) の再起動ルートにフォールバック。

実装ステップ
- applySkipStep と applyMode を追加し、必要な範囲の再設定を行う。
- スレッド同期に注意し、測定ループと排他制御する。

検証
- 変更適用時のフレームドロップ時間を観測し、許容範囲内であること。

完了条件
- 最低限、失敗時は安全に再起動へ委譲される。

---

08. サンプル設定ファイル configs/default.yaml

背景
- 実行に必要なサンプル設定が未整備。読み込みは既に実装済み: [load_app_config()](src/config/config.cpp:7)。

目的
- すぐ起動できる最小構成の例を提供し、設定項目の意味をコメントで明示する。

影響範囲
- 追加ファイル: [configs/default.yaml](configs/default.yaml)。
- main の引数処理: [--config パース](src/main.cpp:16) と既定パス使用 [cfgPath 初期値](src/main.cpp:12)。

方針
- 1台の hokuyo_urg_eth を前提に、UI/DBSCAN/NNG の最小値を記載。
- 将来拡張を見越し、未使用項目もデフォルト値とともにコメント化。

実装ステップ
- YAML を作成し、README 相当の説明コメントを付す。

検証
- 例のまま起動し、UI と WS が動作すること。

完了条件
- 初学者が手戻りなく起動できる。

---

09. パッチ適用とWSの入力検証強化

背景
- 現状は型や範囲の検証が最小限: [applyPatch()](src/core/sensor_manager.cpp:133), [LiveWs::handleSensorUpdate()](src/io/ws_handlers.cpp:119)。

目的
- 不正入力でのクラッシュや想定外再起動を防ぐ。

影響範囲
- 上記2箇所の入力バリデーションとエラーレスポンス。

方針
- 受理するキーのホワイトリスト化、型チェック、範囲チェック、相関チェックを実施。
- 失敗時のエラーコードと説明メッセージを安定化。

実装ステップ
- JSON スキーマ相当の手書き検証を追加。
- 変更が実デバイス再起動を要するかの判定を厳密化。

検証
- フロントから不正値を複数送って挙動確認。

完了条件
- ログに適切なエラーが出力され、状態が破壊されない。

---

10. Web UI の表示強化

背景
- クラスタのみ描画で、原点点群やセンサー別表示は未対応: [webui/app.js](webui/app.js:1)。

目的
- デバッグ効率を上げるため、任意で生点群を描画しセンサー別着色やステータスを表示できるようにする。

影響範囲
- [index.html](webui/index.html), [app.js](webui/app.js), [styles.css](webui/styles.css)。

方針
- UI にトグルを追加し raw 点群の描画をオンオフ可能にする。
- センサーIDに応じた色マップを用い、凡例を表示。
- 接続状態や last seq age の簡易表示を stats に追記。

実装ステップ
- 既存の描画ルーチンに raw 点描画の分岐を追加。
- CSS は既存コンポーネントに合わせて最小限追加。

検証
- UI 操作で描画が切り替わること、FPS が許容範囲に収まること。

完了条件
- デバッグ時に必要な情報が画面上で完結。

---

11. テスト計画

背景
- 重要ロジックに自動テストが無い。

目的
- 回帰を防ぎ、将来の最適化や置換を容易にする。

影響範囲
- 単体: マスク、変換、設定、DBSCAN。
- 疎結合な結合: SensorManager 集約の入出力。

方針
- ユニットテストフレームワークは既存ビルドに合わせて選定（例: Catch2 か GoogleTest）。
- 時間依存は固定データで検証し、副作用を避ける。

実装ステップ
- マスク: ラジアン/度の境界ケースを網羅（[pass_local_mask()](src/core/mask.h:6)）。
- 変換: [apply_pose()](src/core/transform.h:4) の並進回転の組み合わせ。
- 設定: [load_app_config()](src/config/config.cpp:7) の各キー読み取りと正規化。
- DBSCAN: 代表的な配置でクラスタ数と bbox を確認。

検証
- CI でテストが安定して通ること（ローカルでも反復可）。

完了条件
- 主要ユースケースの自動化カバレッジが確保される。

付記
- セキュリティや認可は別ドキュメントで詳細化予定。REST/WS への適用は段階的に行う。