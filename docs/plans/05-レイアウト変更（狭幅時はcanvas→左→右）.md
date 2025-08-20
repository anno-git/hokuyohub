# 実装計画: レイアウト変更（狭幅時は canvas、左バー、右バー の順）

目的
- 幅の狭い画面（ノート/タブレット）で、主目的の表示（Canvas）を最上段に表示し、次いで左ペイン（Sensors/Sinks）、最後に右ペイン（DBSCAN/Filter）の順に縦並びへ変更して操作性を向上する。

対象
- レイアウト・スタイル
  - [webui/styles.css](webui/styles.css)
- マークアップ（必要なら最小限）
  - [webui/index.html](webui/index.html)

現状の把握
- メイン3カラムは以下の構造で実装済み。
  - コンテナ: <div id="main-container" class="main-container">（[webui/index.html](webui/index.html)）
  - 左: <aside id="sidebar-left" class="sidebar-left">
  - 中: <main id="center-canvas" class="center-canvas">
  - 右: <aside id="sidebar-right" class="sidebar-right">
- Canvas 上にはオーバーレイUI（チェックボックス/ボタン類）が存在（[webui/index.html](webui/index.html)）。

変更方針
- DOM順はそのままで、CSSのみでレスポンシブ切替を行う（最小影響）。
- ブレークポイント（例）:
  - 1024px 未満: 縦並び（column）で「canvas→左→右」の順
  - 1024px 以上: 従来の横並び（row）
- order と flex-direction の切替で並び順を制御。Canvas は常に先頭（order: 1）、左（2）、右（3）。
- 縦並び時、Canvas 高さやオーバーレイの余白を若干調整して操作性を確保。

具体手順
1) CSS にレスポンシブルールを追加
   - 対象: [webui/styles.css](webui/styles.css)
   - 追加案（概念）:
     - .main-container { display:flex; flex-direction: row; }
     - @media (max-width: 1024px) {
       - .main-container { flex-direction: column; }
       - #center-canvas { order: 1; }
       - #sidebar-left  { order: 2; }
       - #sidebar-right { order: 3; }
       - #center-canvas { min-height: 50vh; } // 操作性のため高さ確保（調整可）
       - .canvas-overlay { padding: 8px 10px; } // 余白微調整
       - .sidebar-left, .sidebar-right { width: auto; } // 縦では幅制約解除
     - }
   - 既存の .sidebar-left/.sidebar-right の固定幅やマージンがある場合、@media 内で上書きする。

2) オーバーレイやフォームの余白・フォントの微調整（任意）
   - 狭幅時、.overlay-row の改行・縦積みが見やすいように gap や wrap を調整。
   - 右ペイン（特に Filter/DBSCAN）の見出し・行間を調整し、折りたたみUI（別計画 04）と組み合わせて可読性を確保。

3) 必要であれば index.html にアクセシビリティ補助（任意）
   - レスポンシブ対応自体はCSSのみで完結するため、HTML改変は原則不要。

4) 既存スクリプトとの整合性
   - Canvas リサイズは [resize()](webui/app.js:85) で center-canvas のサイズに追従するため、縦並びでも問題ない。
   - ダブルクリックリセット/ズーム/パンなどのハンドラは既存通り動作（[webui/app.js](webui/app.js:2055), [webui/app.js](webui/app.js:2076)）。

受け入れ基準（テスト観点）
- 画面幅を 1024px 未満にすると、見た目が「Canvas → 左 → 右」の縦並びに切り替わる。
- 1024px 以上では従来通りの横並びに戻る。
- 縦並び時でも Canvas のパン/ズーム/ROI 操作、左/右ペインのフォーム編集が支障なく行える。
- オーバーレイのテキストやボタンが折り返しつつも視認性を保つ。

非機能・パフォーマンス
- CSS切替のみのため、描画・処理負荷の増加は無し。
- 縦並び時のスクロールはブラウザ標準の挙動で快適に動作。

ロールバック手順
- 追加した @media ブロックを styles.css から削除/コメントアウト。

作業粒度・変更規模
- 小（CSSの追加が中心、HTML/JSへの影響なし）。

関連ファイル（参照）
- レイアウト: [webui/index.html](webui/index.html)
- スタイル: [webui/styles.css](webui/styles.css)
- Canvas リサイズ: [resize()](webui/app.js:85)

想定工数
- 実装・セルフテスト: 0.5h
- UI微調整: 0.5h