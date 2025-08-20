# Hokuyo-hub WebUI 分割・整理 作業指示書

## 概要（ゴール・前提）

-   **目的**: 巨大な `app.js`
    を**機能別モジュール**へ分割し、見通し・保守性・回帰耐性を高める。\
-   **範囲**: クライアント側 WebUI（現 `app.js`）のみ。サーバ API/WS
    仕様は維持。\
-   **配置場所**: すべての新規 JS は **`webui/js`** 配下に格納。`app.js`
    は薄いエントリに縮小。\
-   **方針**: "**動くまま段階的に**"
    を最優先。小刻みコミット/リグレッション最小化。

------------------------------------------------------------------------

## 期待成果（完成定義 / Doneの状態）

1.  `webui/js` 配下に責務単位のモジュールが作成・分離され、ESM
    で読み込まれる。\
2.  `ws.onmessage`
    は**単一箇所**で定義・ディスパッチされる（多重上書きは消滅）。\
3.  キャンバス描画は**1フレーム1回**に調停され、`requestAnimationFrame`
    で集約。\
4.  UI（センサー/フィルタ/DBSCAN/Sink/ROI）の**イベント登録は各モジュールの
    `init()`** に集約。\
5.  既存の操作（ドラッグ移動/回転/ROI編集/保存・読込/Publishers表示等）は**回帰なしで動作**。\
6.  主要状態（`sensors`, `worldMask`, `viewport`, `rawPoints`,
    `filteredPoints`, `clusterItems`, `currentFilterConfig`,
    `currentDbscanConfig`）は**一元ストア**経由でアクセス。\
7.  基本の E2E 手動チェック（下記「受け入れテスト」）が通る。

------------------------------------------------------------------------

## ディレクトリ構成（最終イメージ）

    webui/
      index.html 他（既存）
      js/
        app.js            // エントリ（初期化の順序だけ）
        store.js          // 状態一元化＋購読API
        ws.js             // WS接続・再接続・送受集中・dispatch
        canvas.js         // 描画・座標変換・再描画調停
        sensors.js        // センサーUI/モーダル/操作（Accordion含む）
        filters.js        // Prefilter/Postfilter UI・サーバ同期
        roi.js            // ROI作成/編集/送受
        dbscan.js         // DBSCAN UI・サーバ同期
        sinks.js          // Sink/Publisher UI・設定変更
        configs.js        // Save/Load/Import/Export RESTラッパ
        api.js            // fetch共通ラッパ（JSON化/エラー整形/タイムアウト）
        utils.js          // 通知・デバウンス等のユーティリティ
        bus.js            // 任意：簡易PubSub（循環依存回避用）
        types.js          // 受信/送信メッセージのスキーマ定義

------------------------------------------------------------------------

## 実装詳細（作業手順）

### 0) 事前準備

-   `index.html` の `<script>` を **`type="module"`** に。読み込み順は
    `app.js` のみ。\

-   `app.js` から各モジュールを import して `init()`
    を呼ぶ構成に移行する。

    ``` js
    // webui/js/app.js
    import * as store from './store.js';
    import * as canvas from './canvas.js';
    import * as ws from './ws.js';
    import * as sensors from './sensors.js';
    import * as filters from './filters.js';
    import * as roi from './roi.js';
    import * as dbscan from './dbscan.js';
    import * as sinks from './sinks.js';
    import * as configs from './configs.js';

    (function bootstrap(){
      store.init();
      canvas.init();       // DOM/ctx確保・描画ループ準備
      sensors.init();
      filters.init();
      roi.init();
      dbscan.init();
      sinks.init();
      configs.init();
      ws.connect();        // 最後に接続、スナップショット要求はここから
    })();
    ```

### 1) `utils.js`

...（中略：前回答の内容をそのまま記載）...

------------------------------------------------------------------------

## 注意点（落とし穴と回避策）

-   **`ws.onmessage` 多重上書き**：必ず単一点ディスパッチに統合。\
-   **描画呼び出し乱発**：`requestRedraw()` に統一し、rAFで調停。\
-   **循環依存**：双方向 `import` を避ける。共有は
    `store`/`ws`/`utils`/`bus` に集約。\
-   **DOM参照の取り直し**：再レンダリングで要素が消えるため、**初期化時に一度だけ参照**し、差分更新で対応。\
-   **フォーカス/アコーディオン状態ロスト**：再描画前後で保存→復元を各UIモジュール内で徹底。\
-   **大量点群時のFPS低下**：ビューポートカリング＋間引きは `canvas.js`
    に一本化して適用。\
-   **WS再接続**：指数バックオフ・手動再接続 API を `ws.js`
    に用意（UIで状態表示）。\
-   **スキーマ不一致**：不正ペイロードは警告ログ＋安全に無視。クラッシュさせない。

------------------------------------------------------------------------

## 受け入れテスト（手動）

1.  起動直後：ステータス表示・FPS・グリッド表示・`sensor.snapshot`
    反映。\
2.  点群：`raw-lite`/`filtered-lite`
    の表示切替、センサー別色の凡例表示。\
3.  センサー：ドラッグで移動、`R`
    回転、フォーム値更新・送信、モーダル編集・保存/Cancel。\
4.  ROI：Include/Exclude の作成・移動・頂点編集・削除、`Enter`/`Esc`
    操作、サーバ更新反映。\
5.  フィルタ/DBSCAN：パラメータ編集→デバウンス送信→サーバ応答でUI値が維持/更新。\
6.  Sinks：一覧表示・有効/無効切替・編集・追加・削除。\
7.  設定：Save/Load/Import/Export
    が成功し、スナップショット再取得でUIに反映。\
8.  パフォーマンス：大量点群（\>10k）でカリング/間引きが効き、UI操作に引っかかりがない。
