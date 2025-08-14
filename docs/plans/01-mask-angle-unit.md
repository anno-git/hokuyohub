# 01 角度単位バグ修正（マスク判定）

概要
- 集約ループで算出する角度はラジアンである一方、マスクしきい値は度数法で格納されています。そのため現在の比較は単位不整合です。これを整理し、統合します。

背景と現状
- 角度の算出はラジアン: [step_to_angle_rad 呼び出し](src/core/sensor_manager.cpp:287)。
- マスクは度数法: [SensorMaskLocal](src/config/config.h:21)。
- 判定関数: [pass_local_mask()](src/core/mask.h:6)。

目標
- 角度比較の単位を統一し、意図した角度範囲のみを通過させる。

変更方針
- 人間が見たり設定したりする値は度
- プログラム中では、ライブラリが度を要求する場合を除いてラジアン

実装ステップ
1. [pass_local_mask()](src/core/mask.h:6) 内で angle_deg = angle * 180.0 / π を計算し、m.angle.min_deg/max_deg と比較する。
2. -180..180 の範囲比較は設定ロード側で正規化済みのため追加のラップ処理は不要: [load_app_config() 角度正規化](src/config/config.cpp:54)。
3. 変更後の関数コメントに単位を明記する。

影響範囲
- 判定関数 [pass_local_mask()](src/core/mask.h:6) のみ。
- 呼び出し側（集約ループ）: [SensorManager::start() 集約ループ](src/core/sensor_manager.cpp:283) に変更は不要。

テスト方針
- 単体テスト（境界ケース含む）
  - min=0, max=10 で 5° が通過し、-5° と 15° が除外される。
  - min=-10, max=10 に対して ±10° 境界の包含/排他仕様を固定（包含推奨）。
  - 距離マスクの near/far は副作用がないこと。
- 性能回帰
  - 変換コストは軽微だが、ホットパスであるため簡易ベンチで劣化がないことを確認。

検証手順（手動）
- 設定で angle[min_deg,max_deg] を狭い範囲にし、可視化で当該扇形外の点が描画されないことを確認。
- ログに余計な警告やエラーが出ないこと。

完了条件
- 既知設定で期待どおりの通過/除外が行われる。
- テストがグリーン。
- コード内コメントに単位が明記されている。

リスクと対応
- ラジアン/度の二重変換ミス: 関数内コメントとテストで担保。
- 将来的にデバイス側設定 first/last を度で扱う改善を行う場合は、本方針と整合するように注意: [HokuyoSensorUrg::openAndConfigure()](src/sensors/hokuyo/HokuyoSensorUrg.cpp:55)。

参照
- [pass_local_mask()](src/core/mask.h:6)
- [SensorMaskLocal](src/config/config.h:21)
- [load_app_config()](src/config/config.cpp:7)
- [SensorManager::start() 集約ループ](src/core/sensor_manager.cpp:283)