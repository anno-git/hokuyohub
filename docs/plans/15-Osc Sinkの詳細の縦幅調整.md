# 実装計画: Osc Sink の詳細の縦幅が足りない問題の改善

目的
- Sinks セクションで OSC タイプの詳細フォームが折りたたみ内に収まらず操作しづらい問題を解消する。
- 小画面や縦レイアウト時（計画 05）でも全項目が見える/操作できるようにする。

対象
- スタイル調整（優先）
  - [webui/styles.css](webui/styles.css)
- 必要に応じたマークアップ/JS 最小調整
  - Sinks 詳細DOM構造: [renderSinks()](webui/app.js:3004)
  - アコーディオンの開閉制御: [renderSinks() 内の toggleAccordion](webui/app.js:3109)

現状の把握
- Sink 詳細は .accordion-item 内の .accordion-content に描画され、初期は collapsed でトグル開閉。
- OSC は以下のフィールドを持つ:
  - in_bundle（checkbox）, bundle_fragment_size（number）など（[renderSinks() 内の type==='osc' 分岐](webui/app.js:3096)）
- 既定スタイルで .accordion-content の max-height や overflow が制約されている可能性。
- 狭幅時レイアウト（計画 05）と併せ、詳細の縦伸び/スクロールを許可するのが適切。

変更方針
- .accordion-content の高さ制限を撤廃、または内容に応じて伸長/内部スクロールを可能にする。
- スクロールが必要な場合は .accordion-content 内部にコンテナ（例: .accordion-content-inner）を設け、そこへ overflow-y: auto を付与する。
- 長いURL等で横スクロールが発生しないよう折返し/省略を適用。
- 小画面時はフォーム行の縦積みや余白最適化で視認性向上。

具体手順

1) CSS の改善
- 対象: [webui/styles.css](webui/styles.css)
- 推奨ルール例:
  - .accordion-content { display:block; max-height: none; padding: 8px 10px; }
  - .accordion-content .accordion-form { display: grid; grid-template-columns: 1fr; row-gap: 8px; }
  - .accordion-form-row { display: grid; grid-template-columns: 120px 1fr; align-items: center; column-gap: 8px; }
  - .accordion-form-row input[type=text],
    .accordion-form-row input[type=number],
    .accordion-form-row select { width: 100%; min-width: 0; }
  - URL 等の長文に対して:
    - .accordion-form-row input[type=text] { overflow: hidden; text-overflow: ellipsis; }
- もし既存で .accordion-content に max-height/overflow 指定がある場合は @media も含めて上書き。

2) 内部スクロール（必要な場合のみ）
- 詳細の縦に極端に長いケース向け（多項目の将来拡張想定）。
- .accordion-content 内側に .accordion-content-inner のラッパを導入（JSテンプレート修正）。
- CSS 例:
  - .accordion-content-inner { max-height: 50vh; overflow-y: auto; }
- 初手は「max-height: none」で十分なことが多いため、発現時のみ導入。

3) Sinks ヘッダのタイトル省略（関連）
- 既に計画 12 でタイトルに URL を併記するため、長い URL で折返し/溢れ対策を CSS に追加:
  - .accordion-header-title { white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
- 対象: [webui/styles.css](webui/styles.css)

4) 小画面レイアウト調整（補完）
- 計画 05 と整合させ、狭幅時にフォーム列を1列表示に変更:
  - @media (max-width: 1024px) {
      .accordion-form-row { grid-template-columns: 1fr; row-gap: 4px; }
      .accordion-form-row label { margin-bottom: 2px; }
    }

受け入れ基準（テスト観点）
- OSC 詳細を展開した際、全項目が視認・操作できる（スクロールなし or 内部スクロールで到達可能）。
- 長い URL がヘッダで視認性を損なわない（省略表示/折返し）。
- 小画面（スマホ/タブレット相当）でも項目が途切れず、操作性が維持される。
- 他タイプ（NNG）の詳細表示にも悪影響がない。

非機能・アクセシビリティ
- フォーム要素のフォーカスリング保持、Tab移動の順序は視覚的レイアウト変更後も自然であること。
- スクロール導入時はコンテンツの終端までキーボードで到達できること。

ロールバック手順
- 追加/変更した CSS ルールをコメントアウトまたは削除し、従来のスタイルへ戻す。

関連リンク
- Sinks 描画・詳細DOM: [renderSinks()](webui/app.js:3004)
- OSC フィールド描画箇所: [renderSinks() type==='osc'](webui/app.js:3096)
- タイトル: [header.innerHTML タイトル組立て](webui/app.js:3042)
- スタイル: [webui/styles.css](webui/styles.css)

想定工数
- 実装/微調整: 0.5h
- 端末幅ごとの見え方検証: 0.5h