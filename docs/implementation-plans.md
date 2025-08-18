# HokuyoHub 実装方針集

この文書は、現在のTODO一覧の各項目について実装の方針を示します。具体実装は担当者に委ねます。
作業担当者は実装計画を日本語でdocs/plansディレクトリにmdファイルで作成し、管理者の承認を得てから作業を開始してください。

目次
- Sink情報をブラウザに表示

---

Sink情報をブラウザに表示

修正・実装する項目
- 現在のSink先の情報をブラウザ上に追加する

実装計画: [docs/plans/2025-08_sink-info-to-browser.md](plans/2025-08_sink-info-to-browser.md)

実装状況:
- ✅ RESTスナップショットAPIに `publishers.sinks` 配列を追加（後方互換性維持）
- ✅ WebSocketスナップショットに `publishers.sinks` 配列を追加
- ✅ WebUIに「Publishers」パネルを追加し、配列データを表示
- ✅ WebSocket経由での自動更新（手動更新/ポーリング不要）
- ✅ テストスクリプトに `publishers.sinks` 配列の検証を追加