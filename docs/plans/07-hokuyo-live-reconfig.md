# 07 Hokuyo ドライバの動的設定反映 実装方針

概要
- センサー動作中に一部設定（mode, skip_step 等）を再起動せず反映できるようにします。デバイスAPIの制約上、完全なオンライン反映が難しい場合は短時間の測定停止→再設定→再開でダウンタイムを最小化します。

関連箇所
- ランタイム適用用フック: [ISensor::applySkipStep](src/sensors/ISensor.h:26), [ISensor::applyMode](src/sensors/ISensor.h:27)
- 実装クラス（未定義）: [HokuyoSensorUrg](src/sensors/hokuyo/HokuyoSensorUrg.h:12)
- 既存のライフサイクル: [HokuyoSensorUrg::start()](src/sensors/hokuyo/HokuyoSensorUrg.cpp:19), [HokuyoSensorUrg::stop()](src/sensors/hokuyo/HokuyoSensorUrg.cpp:33)
- デバイス設定/再接続: [HokuyoSensorUrg::openAndConfigure()](src/sensors/hokuyo/HokuyoSensorUrg.cpp:44), [HokuyoSensorUrg::rxLoop()](src/sensors/hokuyo/HokuyoSensorUrg.cpp:90)
- 呼び出し側: [SensorManager::applyPatch()](src/core/sensor_manager.cpp:199)（フラグに応じてドライバへ委譲 or 再起動）

対象設定
- mode: 強度あり/なしの切替（"ME" or "MD"）→ [toUrgMode()](src/sensors/hokuyo/HokuyoSensorUrg.cpp:84) で urg の測定種別に変換
- skip_step: デバイスのスキャンパラメータ（角度分解能の間引き）→ [urg_set_scanning_parameter](src/sensors/hokuyo/HokuyoSensorUrg.cpp:65) 経由

設計方針
- スレッド安全性: 測定ループ [rxLoop()](src/sensors/hokuyo/HokuyoSensorUrg.cpp:90) と設定反映の競合を避けるため、反映処理時は測定を一時停止する。cb_ 保護用の [cb_mu_](src/sensors/hokuyo/HokuyoSensorUrg.h:37) とは別に、設定変更用の軽量ロック（もしくは running_ の一時 false 化）で同期する。
- 反映手順（共通）
  1) 測定停止（urg_stop_measurement）
  2) 必要な設定 API 呼び出し（skip_step → [urg_set_scanning_parameter](src/sensors/hokuyo/HokuyoSensorUrg.cpp:65)、mode → 測定種別の再開始）
  3) 測定再開（urg_start_measurement）
  4) 失敗時は openAndConfigure→start_measurement のワンショット再試行
- 一貫性: cfg_ を更新し、成功/失敗に応じて戻り値（true/false）で [SensorManager::applyPatch()](src/core/sensor_manager.cpp:199) の再起動判定にフィードバックする。

実装ステップ
1. ISensor フックの実装
   - [HokuyoSensorUrg.h](src/sensors/hokuyo/HokuyoSensorUrg.h:17) に `bool applySkipStep(int) override; bool applyMode(const std::string&) override;` を追加。
2. applySkipStep
   - running_ を確認し、false（未開始）なら cfg_ のみ更新して true を返す（次 start 時に反映）。
   - running_ が true の場合は測定一時停止→ [skip_step_ 更新](src/sensors/hokuyo/HokuyoSensorUrg.cpp:62)→ [urg_set_scanning_parameter](src/sensors/hokuyo/HokuyoSensorUrg.cpp:65)→測定再開。
   - 失敗時は openAndConfigure→start_measurement のワンショット再試行。それでも失敗なら false を返し、上位が再起動ルートへ。
3. applyMode
   - [toUrgMode](src/sensors/hokuyo/HokuyoSensorUrg.cpp:84) で測定種別に変換。
   - 測定停止→ start_measurement を新種別で再起動（[rxLoop() 冒頭の開始処理](src/sensors/hokuyo/HokuyoSensorUrg.cpp:92) と同等の手順）。
   - 失敗時は skipStep と同様に再初期化を一度だけ試し、false なら上位へ。
4. 同期/停止設計
   - rxLoop がループ内で測定失敗時の再接続処理をもつため、apply系の短時間停止と衝突しないようにフラグや軽量ロックで排他。
   - 実装簡素化のため、apply呼び出し中は rxLoop での再接続ロジックを早期 continue ではなく停止状態へ誘導するのも一案。

エッジケース
- 大きな skip_step 変更でステップ範囲 end_step 近似が変化: [end_step 計算](src/sensors/hokuyo/HokuyoSensorUrg.cpp:160) の整合を維持（skipStep 変更後も整合）。
- 連続 apply 呼び出し: 前回の再開完了前に新規要求が来ないように上位で直列化（[SensorManager::applyPatch()](src/core/sensor_manager.cpp:133) 側のロジックで同一 id の連続パッチを順次処理する前提）。

ログ/可観測性
- 反映開始/成功/失敗を INFO/ERROR で出力（センサID/スロット番号がわかる文言）。
- 再初期化が発生した場合もログに残す。

検証
- 実機/スタブで mode, skip_step を変更し、フレーム断の時間を観測（目標: 数百 ms 以内）。
- センサーが無応答の場合に applyXXX が false を返し、[SensorManager::applyPatch()](src/core/sensor_manager.cpp:223) が再起動にフォールバックすること。

完了条件
- 正常系で再起動せず変更が反映される（短時間停止内）。
- 異常系で false が返り上位再起動が機能し、状態が破綻しない。