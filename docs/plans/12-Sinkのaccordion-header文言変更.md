# 実装計画: Sink の accordion-header 文言を変更（例: NNG(tcp://127.0.0.1:5555)）

目的
- Sinks 一覧の見出しに「種別 + URL」を併記し、視認性と識別性を向上する。
- 例: `NNG(tcp://127.0.0.1:5555)` または `OSC(udp://192.168.0.2:9000)`

対象
- WebUI（レンダリング箇所）
  - Sinks リスト描画: [renderSinks()](webui/app.js:3004)
  - ヘッダHTML構築: [header.innerHTML のタイトル組立て](webui/app.js:3042)

現状の把握
- 現在のヘッダタイトルは `${type.toUpperCase()} Sink ${i}` として描画されている。
  - 該当箇所: [webui/app.js](webui/app.js:3042)
- URL/その他情報はヘッダの右側カラムや詳細内に存在するが、見出しには含まれない。

変更方針
- ヘッダのタイトルテキストを `${TYPE}(${URL})` に変更。
  - TYPE: 'nng' | 'osc' を大文字化（NNG/OSC）
  - URL: sink.url を利用。長すぎる場合は一部省略（任意）
- URL が未設定または空の場合は `${TYPE}` のみ表示。
- 将来 topic を併記したければ `${TYPE}(${URL}) — ${topic}` 形式も検討可（今回は URL のみ）。

具体手順
1) タイトル文字列の生成変更
   - 対象: [renderSinks()](webui/app.js:3024)
   - `const type = sink.type || 'unknown';`
   - `const url  = sink.url || '-';`
   - 新しいタイトル:
     - `const title = url && url !== '-' ? \`\${type.toUpperCase()}(\${url})\` : \`\${type.toUpperCase()}\`;`
   - ヘッダ HTML 内の `.accordion-header-title` に `title` を埋め込む。

2) 長いURLの省略（任意）
   - CSS 側で `.accordion-header-title` に対し、`white-space: nowrap; overflow: hidden; text-overflow: ellipsis; max-width: ...;` を指定。
     - 対象: [webui/styles.css](webui/styles.css)

3) テスト観点
   - NNG/OSC 両方で期待表示となる。
   - URL が未設定の sink でもエラーとならず TYPE のみが表示される。
   - トグル・削除等のヘッダ操作に影響がない。

受け入れ基準
- 例: type=nng, url=tcp://127.0.0.1:5555 → ヘッダに `NNG(tcp://127.0.0.1:5555)` と表示。
- エラーやコンソール警告無し。
- レイアウト崩れ無し（長いURL時は省略表示）。

ロールバック手順
- ヘッダタイトルの生成を元の `${type.toUpperCase()} Sink ${i}` に戻す。

関連ファイル（参照）
- レンダリング: [renderSinks()](webui/app.js:3004)
- タイトル組立て位置: [header.innerHTML](webui/app.js:3042)
- スタイル（省略時対応）: [webui/styles.css](webui/styles.css)

想定工数
- 実装/確認: 0.3h