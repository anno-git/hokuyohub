# 02 ダミークラスタ送出を DBSCAN 出力へ置換

概要
- 現状は集約フレーム受信時に固定のダミークラスタを送出しているため、実データに基づくクラスタをフロント/バスへ配信するように置換する。

関連箇所
- ダミークラスタ送出: [main.cpp](src/main.cpp:67)
- DBSCAN インターフェイス: [DBSCAN2D::run()](src/detect/dbscan.cpp:4), 宣言 [dbscan.h](src/detect/dbscan.h:9)
- WS 送出: [LiveWs::pushClustersLite()](src/io/ws_handlers.cpp:78)
- バス送出: [NngBus::publishClusters()](src/io/nng_bus.cpp:9)
- 集約出力型: [ScanFrame 構造](src/core/sensor_manager.h:9)

目的
- 集約された点群 ScanFrame.xy/sid に対して DBSCAN を実行し、得られた Cluster をそのまま WS と NNG に流す。

方針
- main のセンサコールバック内で、渡された ScanFrame をそのまま DBSCAN に入力する。
- DBSCAN のパラメータは設定済みインスタンス（eps, minPts）を利用: [DBSCAN2D 生成](src/main.cpp:38)。
- DBSCAN の出力 Cluster は既存WSフォーマットと同一構造であるため、変換は不要（id, sensor_mask, bbox, 重心, count）。
- 例外や空クラスタの扱いは静かにスキップ（UI は items.length=0 を許容）。

実装ステップ
1. main のコールバックでダミーデータを削除し、DBSCAN 実行へ置換。
   - ScanFrame.xy は [x0,y0,x1,y1,...] のフラット配列、sid は各点のセンサーID（0..255）。
   - [DBSCAN2D::run()](src/detect/dbscan.cpp:4) は `std::span` で受け取るためコピー不要。
2. 返却された std::vector<Cluster> を WS と NNG へ送出。
   - WS: [LiveWs::pushClustersLite()](src/io/ws_handlers.cpp:78)
   - NNG: [NngBus::publishClusters()](src/io/nng_bus.cpp:9)
3. DBSCAN が未実装の場合は一時的に空配信（items=0）でも成立するが、MVP完了にはタスク 03 の実装が必要。

注意点
- フレームごとのメモリアロケーションを最小化する（DBSCAN 実装側でバッファ再利用が望ましい）。
- 例外安全: run() で例外が投げられうる実装に備え try/catch で落ちないようにする（ログのみ）。
- 負荷: 30 FPS を目標とし、run() 内部のアルゴリズムで性能を確保（タスク 03）。

検証
- 点群が存在する状態で UI にクラスタ点と bbox が表示され、seq/t が更新される。
- 空のとき items=0 が安定して配信される。
- run() のパラメータ（eps, minPts）を設定変更すると反映される。

完了条件
- ダミークラスタ送出が完全に除去され、実データに基づくクラスタのみがフロント/バスへ送られる。
- ログにエラーが出ない（DBSCAN 未実装期間中は items=0 で平常に動作）。

将来拡張
- クラスタのID連番はフレーム内で 0..N-1 でもよいが、トラッキングを行う場合は別ID付与戦略を検討。
- NNG 側のスキーマ固定化とバージョニング（タスク 05 と連動）。