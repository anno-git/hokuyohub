# 実装計画: ROI の数値編集を可能にする

目的
- 現在はキャンバス上でのドラッグ操作やキーボード操作（Insert/Delete）により ROI 頂点の編集が可能だが、数値精度での微調整ができない。
- 右ペインに「ROI エディタ」を追加し、選択中の ROI の各頂点座標を数値で編集できるようにする。編集は即座にキャンバスへ反映し、サーバへ world.update を送る。

対象
- WebUI（UI/編集ロジック）
  - ROI 選択/編集状態: [findROIAt()](webui/app.js:1813), [findVertexAt()](webui/app.js:1833)
  - ROI 配列: worldMask.include / worldMask.exclude（描画 [drawROI()](webui/app.js:572)）
  - 変更送信: [sendWorldMaskUpdate()](webui/app.js:2213)
  - スナップショット反映: [loadWorldMaskFromSnapshot()](webui/app.js:2230)
  - 更新受信: world.updated ハンドリング（[ws.onmessage 追加分岐](webui/app.js:2675)）
- サーバ（適用ロジック）
  - world.update の受信/適用: [handleWorldUpdate()](src/io/ws_handlers.cpp:313)

現状の把握
- ROI は worldMask.include / exclude に、ポリゴン配列（[[x,y], [x,y], ...]）として保持。
- 選択/頂点選択はキャンバス操作で可能（selectedROI, selectedROIType, selectedVertex）。
- ROI 変更後に [sendWorldMaskUpdate()](webui/app.js:2213) を呼び、サーバが [handleWorldUpdate()](src/io/ws_handlers.cpp:313) で AppConfig.world_mask に反映し、全接続へ world.updated をブロードキャストする。
- 現在 UI に数値編集フォームは存在しない。

変更方針
- 右サイドバー（sidebar-right）に「ROI Editor」セクションを新設し、選択中 ROI の
  - 種別（Include / Exclude）
  - インデックス（i）
  - 頂点テーブル（各行 x, y の number 入力）
  - 頂点追加/削除ボタン
  を表示する。
- 入力 change/blur 時に worldMask の座標を即時更新し、[sendWorldMaskUpdate()](webui/app.js:2213) を呼ぶ。
- 頂点の追加/削除は GUI からも可能にする（既存の Insert/Delete の GUI 版）。
- バリデーション（NaN/Infinity、頂点数 3 未満は不可）を実装。
- 任意: スナップ（丸め）オプション（例: 0.01m 刻み）を設ける。

UI 具体仕様
- 配置: 右ペインの Filter/DBSCAN の下部に「ROI Editor」セクションを追加。
  - セクション見出しは折りたたみ可能（Filter/DBSCAN と同じアコーディオン UX）。
- 表示要素:
  - 選択状態: 「Type: Include/Exclude, Index: n」
  - 頂点テーブル: 1 行 = 頂点 i、入力 x, y（number, step=0.01）
  - 行末に「+（この後に追加）」「🗑️（削除）」ボタン
  - 下部に「Add Vertex（末尾追加）」ボタン
- 状態同期:
  - キャンバスで ROI/頂点を選択・編集した場合もテーブルへ反映（双方向同期）。
  - world.updated を受けた場合にテーブル再構築。
- 操作ガード:
  - 頂点 3 未満となる削除操作は禁止（エラーメッセージ/無効化）。

実装手順
1) HTML 追加（index.html）
   - 右ペイン（#sidebar-right）内に新規セクションを追加。最小限のラッパとプレースホルダ要素のみ（JS で中身を生成）。
     - 例: 
       - <section id="roi-panel">
         - <div class="panel-header"><h2>ROI Editor</h2><span id="roi-msg" class="panel-msg"></span></div>
         - <div id="roi-editor" class="roi-editor"></div>
       - </section>

2) JS ロジック（app.js）
   - 参照取得: const roiEditor = document.getElementById('roi-editor');
   - レンダ関数: renderRoiEditor()
     - selectedROI / selectedROIType が null の場合は「No ROI selected」表示。
     - 選択ありの場合、対象ポリゴンを取得しテーブル描画。
     - 各入力に change/blur リスナを付けて worldMask を即時更新 → [sendWorldMaskUpdate()](webui/app.js:2213) → [redrawCanvas()](webui/app.js:216)
     - 「+」「🗑️」ボタン:
       - 追加: [addVertexToROI()](webui/app.js:1894) を利用、または同等処理→送信
       - 削除: [deleteVertexFromROI()](webui/app.js:1884) を利用→送信
       - 3 頂点未満を許容しない
   - 再描画トリガ:
     - ROI 選択/頂点選択/移動時（mousedown/mousemove/mouseup）に renderRoiEditor() を呼ぶ（パフォーマンスに注意して必要時のみ）。
       - 例: ROI/頂点確定（mouseup）時、選択変更時に呼ぶ
     - world.updated と sensor.snapshot(world_mask 含む) 受信時に renderRoiEditor() を呼ぶ（[ws.onmessage 追加分岐](webui/app.js:2675)）
   - メッセージ表示: setRoiMessage(text, isError) のヘルパを作成（3 秒クリア）

3) サーバ側（確認）
   - [handleWorldUpdate()](src/io/ws_handlers.cpp:313) は includes/excludes を完全置換で反映済み。既存実装で UI の逐次変更に対応可能。
   - エラーハンドリング: 不正 JSON の場合 error を返すが、UI では正常更新のみ送るため基本不要。

4) バリデーション
   - JS 入力値を parseFloat し、isFinite をチェック。無効値は直前値へロールバックしエラー表示。
   - 頂点 3 未満となる削除要求は無効化（disabled）または警告。

5) 任意: スナップ/丸め
   - 入力の step=0.01 に加え、内部的に (Math.round(v*100)/100) 丸めを適用可能。
   - トグル可能とするなら、ROI Editor に「Snap 0.01m」チェックボックスを追加。

受け入れ基準（テスト）
- ROI を選択すると ROI Editor に頂点一覧が表示され、数値編集が可能。
- 数値変更がキャンバスと他ブラウザへ反映（world.updated ブロードキャスト）。
- 頂点追加/削除が GUI から可能で、3 点未満への削除は不可。
- 無効値（文字列/Infinity/NaN）は拒否し、UI にエラー表示。
- キャンバスでの操作（ドラッグ/Insert/Delete）もエディタへ即時反映。

非機能・アクセシビリティ
- 入力はキーボードのみで完結可能。
- 小画面時もテーブルが収まるよう、行内折返し/スクロールを CSS で調整（.roi-editor 内部に overflow-y:auto）。

ロールバック手順
- index.html から ROI パネルを削除、app.js の renderRoiEditor() 呼び出しを削除。

作業粒度・変更規模
- 中（UI セクション追加＋双方向同期の実装）。

関連リンク
- ROI 選択: [findROIAt()](webui/app.js:1813) / 頂点選択: [findVertexAt()](webui/app.js:1833)
- ポリゴン描画: [drawROI()](webui/app.js:572)
- 送信: [sendWorldMaskUpdate()](webui/app.js:2213)
- スナップショット反映: [loadWorldMaskFromSnapshot()](webui/app.js:2230)
- サーバ適用: [handleWorldUpdate()](src/io/ws_handlers.cpp:313)