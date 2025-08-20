# 実装計画: Sensor 詳細保存後の挙動不具合（angle step 等）の修正

目的
- Advanced Settings... → Save 後にセンサー挙動（特に scan/angle step 周り）が乱れる不具合を解消する。
- UI パッチのキー体系/単位とサーバ側適用（SensorManager.applyPatch）の期待値を厳密に整合させ、バリデーション/正規化/適用順を明確化する。

対象
- WebUI 側（詳細モーダルのパッチ生成/送信）
  - センサー詳細モーダル/保存: [webui/app.js](webui/app.js)
- WebSocket/REST サーバ適用経路
  - WS: [src/io/ws_handlers.cpp](src/io/ws_handlers.cpp)
  - REST: [src/io/rest_handlers.cpp](src/io/rest_handlers.cpp)
- センサー内部適用
  - SensorManager（パッチ適用・再起動・角度/スキップステップ反映）: [src/core/sensor_manager.cpp](src/core/sensor_manager.cpp), [src/core/sensor_manager.h](src/core/sensor_manager.h)
  - センサー生成/ファクトリ: [src/sensors/SensorFactory.cpp](src/sensors/SensorFactory.cpp)

現状の把握
- UI から送られる PATCH（モーダル）は endpoint/mode/ignore_checksum_error/skip_step/mask(angle/range)/pose(tx,ty,theta_deg) を含む。
  - 送信生成（モーダル）: [webui/app.js](webui/app.js)
- WS 側は `sensor.update` を受けて SensorManager.applyPatch に委譲し、ok/sensor.updated を返す（期待）。
  - 受信処理: [handleSensorUpdate](src/io/ws_handlers.cpp:289)
- REST では PATCH /api/v1/sensors/:id で同様のパッチモデルを適用。
  - 実装: [patchSensor](src/io/rest_handlers.cpp:80)
- 不具合症状として「angle step がおかしい」旨の記述があるため、mask/skip_step/角度関連の適用順・換算・境界条件に起因する可能性が高い。

想定される原因
- UI のキー名/単位とサーバ側の期待が不一致（例: angle[min_deg,max_deg] と内部が [min,max] rad 単位等）。
- skip_step の下限（>=1）未満が入り得る/クリップ漏れ。
- mask.range の near/far, mask.angle の min/max の範囲チェック・正規化（min≤max）漏れ。
- applyPatch の再初期化順序（例: param 更新→デバイス再起動 or 幾何キャッシュ再計算の順）に問題。
- UI の楽観更新とサーバ応答反映の競合で一瞬不整合が発生。

変更方針（全体）
1) パッチスキーマの仕様化と整合チェック
   - UI→Server の JSON キー・型・単位・範囲を仕様書化（本ファイルに記載）。
   - Server 側 applyPatch にバリデーション/クリップ/正規化を実装（エラー時は 400/WS error を返す）。

2) 適用順序と再初期化の明確化
   - pose/mask/skip_step/mode/endpoint の各変更が必要とする副作用（ドライバ再起動、スキャンテーブル再計算など）を整理。
   - 再初期化は「安全な順番」で行う（停止→設定→再始動→内部キャッシュ更新）。

3) UI の送信前チェックと応答反映の厳格化
   - UI 送信前に簡易バリデーション（数値/範囲）。
   - 応答（ok/sensor.updated）側を必ず UI に反映。楽観更新との差異があればサーバ値で上書き。

実装手順（詳細）

A) パッチ仕様の定義（提案）
- pose:
  - pose.tx, pose.ty: float（m）
  - pose.theta_deg: float（degree, -360..360 を 0..360 正規化はサーバ側でも可）
- endpoint:
  - endpoint: { host: string, port: int[1..65535] } または "host:port"（UI では object化）
- mode:
  - "MD" | "ME"（他の値はエラー）
- skip_step:
  - int >= 1（負/0 は 1 にクリップ or エラー）
- ignore_checksum_error:
  - 0/1（bool 解釈）
- mask.angle:
  - { min_deg: float, max_deg: float }（-180..180 を推奨、min<=max 正規化）
- mask.range:
  - { near_m: float>=0, far_m: float>=near }（far はデバイス上限でクリップ）

B) サーバ側の適用強化
- WS: [handleSensorUpdate](src/io/ws_handlers.cpp:289)
  - 受信 JSON の検査を強化（型チェック等）。不正なら error 応答。
  - SensorManager.applyPatch から返る `applied` を ok と共に返し、UI へ「確定値」を明示。
- REST: [patchSensor](src/io/rest_handlers.cpp:80)
  - 同等の検証/エラー応答を実施（現状もエラー応答あり）。
- SensorManager.applyPatch（実装側の改修）: [src/core/sensor_manager.cpp](src/core/sensor_manager.cpp), [src/core/sensor_manager.h](src/core/sensor_manager.h)
  - 変更点:
    1) 取り込み前にすべてのパラメータを validate/normalize
       - skip_step = max(1, skip_step)
       - theta_deg を -180..180 に正規化（例: fmod/ラップ）
       - mask.angle: min_deg ≤ max_deg に並べ替え、範囲外はクリップ（-180..180）
       - mask.range: near_m ≥0、far_m ≥ near_m、デバイス上限でクリップ
    2) 実機/ドライバの再設定が必要な項目（mode/endpoint/skip_step/mask角度範囲等）をグルーピングし、停止→設定→再開の手順を一本化
       - 例: stopAcquisition() → reconfigure(params) → rebuildAngleTable() → startAcquisition()
    3) 角度 step の計算が mask/skip_step に依存する場合、適用後に内部テーブル再計算
    4) `applied` JSON を構築（確定値を書き戻し）し返却

C) UI 側改善（任意）
- 送信前チェック（buildPatchFromModal 内）
  - port 範囲、skip_step≥1、角度/距離の有効性チェック（不正は送信しない・警告）
  - 関連箇所: [buildPatchFromModal](webui/app.js)
- 応答の確定反映
  - ok/sensor.updated の payload 内 sensor を用いてフォーム/キャンバス state を上書き（既に実装されているが、`maybeCloseModalOnAck` と合わせて確実化）
  - 関連箇所: [maybeCloseModalOnAck](webui/app.js), WS onmessage の sensor.updated 分岐（remote/local 処理）

D) デバッグログ/計測
- 角度テーブル生成/skip_step 反映箇所に INFO ログを追加（新旧値の出力）
- エラー時には何が拒否されたか明示（キー名・値・許容範囲）

テスト計画
- 正常系
  - skip_step=1/2/5 の切替でスキャン挙動が安定し、UI の sensor.updated と一致
  - mask.angle の min/max を入れ替え入力（max<min）しても正常化され正しく適用
  - pose.theta_deg を 370 入力 → 10 に正規化される等
- 異常系
  - port=0、skip_step=0、範囲外角度/距離を送信 → エラー応答
  - endpoint を不正形式で送信 → エラー応答
- 再初期化順
  - mode/endpoint/skip_step/angle mask を同時変更しても、停止→設定→開始の順で安定動作すること

ロールバック手順
- SensorManager.applyPatch のバリデーション/正規化を元に戻す（コミット単位）
- UI の事前チェックをコメントアウト
- ログ出力をレベル変更/削除

関連リンク
- WS適用: [handleSensorUpdate](src/io/ws_handlers.cpp:289)
- REST適用: [patchSensor](src/io/rest_handlers.cpp:80)
- センサ管理: [src/core/sensor_manager.cpp](src/core/sensor_manager.cpp), [src/core/sensor_manager.h](src/core/sensor_manager.h)
- センサ生成: [src/sensors/SensorFactory.cpp](src/sensors/SensorFactory.cpp)