# 実装計画: DBSCAN 設定パネルを折りたたみ可能にする

目的
- 右サイドバーの「DBSCAN Configuration」を折りたたみ可能（Accordion）にし、狭い画面でも操作性を高める。
- Filterパネルの折りたたみ実装とUX/ARIAを揃える。

対象
- マークアップ調整: [webui/index.html](webui/index.html)
- スクリプト追加: [webui/app.js](webui/app.js)
- スタイル（必要に応じて）: [webui/styles.css](webui/styles.css)

現状の把握
- DBSCANパネル構造（見出しが h2 固定、折りたたみ要素なし）
  - セクション: [webui/index.html](webui/index.html)
  - 見出し/本体: 
    - h2「DBSCAN Configuration」
    - .dbscan-config（パラメータ群） [webui/index.html](webui/index.html)
- DBSCANのUI/イベントは既に実装済み（値のロード/送信）
  - UI要素取得: [const dbscanElements = {...}](webui/app.js:2778)
  - 送信: [sendDbscanConfig()](webui/app.js:2833)
  - イベント登録: [setupDbscanEventListeners()](webui/app.js:2848)
  - 受信適用: [applyDbscanConfigToUI()](webui/app.js:2823)
  - WS受信ハンドラ: [ws.onmessage DBSCAN分岐](webui/app.js:3593)

変更方針
- Filterパネルのアコーディオン実装（[setupFilterAccordion()](webui/app.js:1667)）と同様の仕組みを DBSCAN に適用。
- h2 を h3+caret（トグル可能）にし、.dbscan-config を .collapsed 切替で表示/非表示。
- ARIA属性（aria-expanded, aria-controls, role=region）を付与。
- 初期状態は「折りたたみ（collapsed）」推奨（狭幅時の視認性向上）。

具体手順
1) HTML 構造の変更
   - 対象: [webui/index.html](webui/index.html)
   - 現在:
     - <h2>DBSCAN Configuration</h2>
     - <div class="dbscan-config">...</div>
   - 変更案:
     - h2 → h3（tabindex=0, role=button, aria-expanded=false, aria-controls=...）
     - caret（▼/▶）を見出し内に配置（span.toggle-caret）
     - .dbscan-config に id を付与（例: dbscan-content）し、role=region を付与
     - 親セクションに .collapsed を初期付与
   - 例（擬似コード）
     - <h3 tabindex="0" role="button" aria-expanded="false" aria-controls="dbscan-content">DBSCAN Configuration <span class="toggle-caret">▶</span></h3>
     - <div class="dbscan-config" id="dbscan-content" role="region">...</div>

2) JS にアコーディオン初期化を追加
   - 対象: [webui/app.js](webui/app.js)
   - 新規: setupDbscanAccordion() を実装（[setupFilterAccordion()](webui/app.js:1667) を参考）
     - ヘッダークリック/Space/Enterで .collapsed の付け外し
     - aria-expanded の更新
     - caret（.toggle-caret）の更新（▼/▶）
   - DOMContentLoaded で setupDbscanAccordion() を呼ぶ（DBSCANリスナ登録後でOK）
     - 追記箇所候補: 既存の DBSCAN 初期化ブロック（[document.addEventListener('DOMContentLoaded', ...)](webui/app.js:3564) 付近）

3) CSS（必要であれば）
   - 対象: [webui/styles.css](webui/styles.css)
   - .collapsed のとき .dbscan-config を display:none または max-height:0 に
   - 既存のフィルタ用アコーディオンと同じ見た目の一貫性を担保

状態保持（任意）
- ユーザの開閉状態を localStorage（キー: dbscanCollapsed=true/false）に保存/復元する処理を追加可能。
- 初回は collapsed=true（折りたたみ）で開始し、ユーザ操作後は最後の状態を復元。

受け入れ基準（テスト観点）
- キーボード（Space/Enter）とクリックで開閉できる（ARIA属性が適切に更新される）。
- 折りたたみ時、DBSCANパラメータ編集が非表示になるが、バックグラウンドで状態が壊れない。
- 展開時、現行の設定値が UI に正しく表示され、編集→[sendDbscanConfig()](webui/app.js:2833) が動作する。
- 画面幅が狭い環境で右ペインのスクロール量が減り、操作がしやすくなる。

非機能・アクセシビリティ
- 見出しに role=button/aria-expanded/aria-controls を付与し、Tab移動/Enter/Spaceで操作可能。
- caret表示で開閉状態が視覚的に分かる。

ロールバック手順
- h3/caretの追加を戻し、.collapsed制御を除去。
- setupDbscanAccordion() の呼び出しを削除。

作業粒度・変更規模
- 小（マークアップ少量変更+JS関数1つ追加）。

関連ソース（参照）
- DBSCAN要素群: [const dbscanElements](webui/app.js:2778)
- 受信適用: [applyDbscanConfigToUI()](webui/app.js:2823)
- 送信: [sendDbscanConfig()](webui/app.js:2833)
- イベント登録: [setupDbscanEventListeners()](webui/app.js:2848)
- 既存のアコーディオン実装参考: [setupFilterAccordion()](webui/app.js:1667)

想定工数
- 実装・セルフテスト: 0.5h
- コードレビュー/微調整: 0.5h