# HokuyoHub

HokuyoHubは、複数のHokuyo LiDARセンサーからのデータを統合し、リアルタイムでクラスタリング処理を行うC++アプリケーションです。WebUIによる可視化とREST APIによる設定管理を提供します。

## 主な機能

### センサー管理
- 複数のHokuyo URG Ethernetセンサーの同時接続・制御
- センサーごとの位置・姿勢設定（tx, ty, theta）
- センサーごとの角度・距離マスク設定
- リアルタイムでのセンサー有効/無効切り替え

### データ処理パイプライン
- **プリフィルター**: ノイズ除去、スパイク除去、外れ値除去、近傍フィルタリング
- **DBSCAN クラスタリング**: 高性能な2Dクラスタリング（正規化距離、角度スケーリング対応）
- **ポストフィルター**: クラスター分離除去、サイズフィルタリング
- **ROI (Region of Interest)**: 包含・除外領域の設定

### データ配信
- **NNG**: MessagePack形式でのリアルタイム配信
- **OSC**: Open Sound Control プロトコル対応
- **WebSocket**: WebUI向けJSON配信
- レート制限、バンドル配信対応

#### NNG メッセージフォーマット

**MessagePack形式（推奨）:**
```
{
  "v": 1,                    // プロトコルバージョン
  "seq": 12345,              // フレームシーケンス番号
  "t_ns": 1640995200000000000, // タイムスタンプ（ナノ秒）
  "raw": false,              // 生データフラグ
  "items": [                 // クラスター配列
    {
      "id": 0,               // クラスターID
      "cx": 1.23,            // 中心X座標 [m]
      "cy": 4.56,            // 中心Y座標 [m]
      "minx": 1.0,           // バウンディングボックス最小X [m]
      "miny": 4.0,           // バウンディングボックス最小Y [m]
      "maxx": 1.5,           // バウンディングボックス最大X [m]
      "maxy": 5.0,           // バウンディングボックス最大Y [m]
      "n": 25                // クラスター内点数
    }
  ]
}
```

**JSON形式（フォールバック）:**
MessagePackエンコードに失敗した場合、同じ構造のJSONで配信されます。

#### OSC メッセージフォーマット

**個別メッセージ形式:**
```
Address: /hokuyohub/cluster
Type Tags: ,itiffffffi
Arguments:
  - id (int32):     クラスターID
  - t_ns (int64):   タイムスタンプ（ナノ秒）
  - seq (int32):    フレームシーケンス番号
  - cx (float32):   中心X座標 [m]
  - cy (float32):   中心Y座標 [m]
  - minx (float32): バウンディングボックス最小X [m]
  - miny (float32): バウンディングボックス最小Y [m]
  - maxx (float32): バウンディングボックス最大X [m]
  - maxy (float32): バウンディングボックス最大Y [m]
  - n (int32):      クラスター内点数
```

**バンドル形式（推奨）:**
複数のクラスターを1つのOSCバンドルにまとめて送信。UDP MTU制限を考慮して自動分割されます。
- NTPタイムタグ付き
- 設定可能なフラグメントサイズ（デフォルト: 1200バイト）

#### WebSocket メッセージフォーマット

**クラスターデータ:**
```json
{
  "type": "clusters-lite",
  "seq": 12345,
  "t": 1640995200000000000,
  "items": [
    {
      "id": 0,
      "cx": 1.23, "cy": 4.56,
      "minx": 1.0, "miny": 4.0,
      "maxx": 1.5, "maxy": 5.0
    }
  ]
}
```

**生データ:**
```json
{
  "type": "raw-lite",
  "seq": 12345,
  "t": 1640995200000000000,
  "xy": [1.0, 2.0, 1.1, 2.1, ...],  // [x0,y0,x1,y1,...]
  "sid": [0, 0, 1, 1, ...]           // センサーID配列
}
```

**フィルター済みデータ:**
```json
{
  "type": "filtered-lite",
  "seq": 12345,
  "t": 1640995200000000000,
  "xy": [1.0, 2.0, 1.1, 2.1, ...],
  "sid": [0, 0, 1, 1, ...]
}
```

### WebUI
- リアルタイムLiDARデータ可視化（Canvas描画）
- インタラクティブなセンサー配置・回転
- ROI領域の作成・編集（ドラッグ&ドロップ）
- フィルター設定のリアルタイム調整
- 設定の保存・読み込み・インポート・エクスポート

## システム要件

### 依存関係
- **C++20** 対応コンパイラ
- **CMake** 3.18以上
- **Drogon** フレームワーク（HTTP/WebSocket）
- **yaml-cpp** （設定ファイル）
- **NNG** （オプション、メッセージング）
- **Hokuyo URG Library** （自動ビルド）

## インストール

### Ubuntu/Debian/Raspberry Pi OS

```bash
# 基本依存関係
sudo apt-get update
sudo apt-get install -y cmake g++ pkg-config git libjsoncpp-dev uuid-dev \
  zlib1g-dev libssl-dev libbrotli-dev

# Drogon フレームワーク
sudo apt-get install -y libdrogon-dev || {
  git clone https://github.com/drogonframework/drogon && cd drogon && \
  git submodule update --init && mkdir build && cd build && \
  cmake .. -DCMAKE_BUILD_TYPE=Release && \
  make -j$(nproc) && sudo make install && cd ../../
}

# YAML-CPP
sudo apt-get install -y libyaml-cpp-dev

# NNG（オプション）
sudo apt-get install -y cmake ninja-build
git clone https://github.com/nanomsg/nng.git && cd nng && mkdir build && cd build \
  && cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DNNG_TESTS=OFF -DNNG_TOOLS=OFF .. \
  && ninja && sudo ninja install && sudo ldconfig
```

### ビルド

#### 従来の方法（引き続き利用可能）

```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_NNG=ON ..
make -j$(nproc)
```

#### CMake Presets を使用した方法（Phase 1 - 推奨）

```bash
# リリースビルド（推奨）
cmake --preset mac-release
cmake --build --preset build-mac-release

# デバッグビルド
cmake --preset mac-debug
cmake --build --preset build-mac-debug

# RelWithDebInfo ビルド（プロファイリング用）
cmake --preset mac-relwithdebinfo
cmake --build --preset build-mac-relwithdebinfo

# 利用可能なプリセット一覧
cmake --list-presets
cmake --list-presets=build
```

#### 便利スクリプトを使用した方法

```bash
# リリースビルド + インストール
./scripts/build_with_presets.sh release --install

# デバッグビルド + インストール
./scripts/build_with_presets.sh debug --install

# 簡単なラッパー
./scripts/preset_build.sh release  # または debug, clean

# ヘルプ表示
./scripts/build_with_presets.sh --help
```

### インストール

#### 従来の方法

```bash
# デフォルトでは ./dist にインストール
make install

# システムワイドインストール
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
make install
```

#### CMake Presets を使用した方法

```bash
# プリセットビルド後のインストール
cmake --install build/darwin-arm64

# または便利スクリプトで一括実行
./scripts/build_with_presets.sh release --install
```

## 使用方法

### 基本実行

```bash
./hokuyo_hub --config ./configs/default.yaml --listen 0.0.0.0:8080
```

### コマンドライン引数

- `--config <path>`: 設定ファイルパス（デフォルト: `./config/default.yaml`）
- `--listen <host:port>`: HTTP/WebSocketサーバーアドレス（設定ファイルで指定可能）

### WebUI アクセス

ブラウザで `http://localhost:8080` にアクセス

#### WebUI 操作方法

**ビューポート操作:**
- ドラッグ: パン
- マウスホイール: ズーム
- ダブルクリック: ビューリセット

**センサー操作:**
- ドラッグ: センサー移動
- `R`キー: 選択センサーの回転

**ROI操作:**
- `+ Include Region`: 包含領域作成開始
- `+ Exclude Region`: 除外領域作成開始
- クリック: 頂点追加
- `Enter`: 領域作成完了
- `Escape`: 作成キャンセル
- ドラッグ: 領域・頂点移動
- `Delete`: 選択領域・頂点削除

## 設定ファイル

### 基本構造（YAML）

```yaml
sensors:
  - id: 0
    name: "hokuyo-a"
    type: "hokuyo_urg_eth"
    endpoint: "192.168.104.202:10940"
    pose: { tx: 0.0, ty: 0.0, theta: 0.0 }
    enabled: true
    mode: "ME"  # "MD"=距離のみ / "ME"=距離+強度
    mask:
      angle: { min: -135.0, max: 135.0 }  # degree
      range: { near: 0.05, far: 15.0 }    # m

world_mask:
  polygon: []  # ROI設定（WebUIで編集可能）

dbscan:
  eps_norm: 5      # 正規化距離閾値
  minPts: 5        # 最小点数
  k_scale: 1       # 角度項スケール係数

prefilter:
  enabled: true
  neighborhood:
    enabled: true
    k: 5                    # 最小近傍点数
    r_base: 0.05           # 基準半径 [m]
    r_scale: 1.0           # 距離スケール係数
  spike_removal:
    enabled: true
    dr_threshold: 0.3      # スパイク検出閾値
    window_size: 3         # ウィンドウサイズ
  # その他のフィルター設定...

postfilter:
  enabled: true
  isolation_removal:
    enabled: true
    min_points_size: 3     # 最小クラスターサイズ
    isolation_radius: 0.2  # 分離判定半径

sinks:
  - { type: "nng", url: "tcp://0.0.0.0:5555", topic: "clusters", encoding: "msgpack", rate_limit: 120 }
  - { type: "osc", url: "0.0.0.0:10000", topic: "clusters", rate_limit: 120 }

ui:
  listen: "0.0.0.0:8080"
```

## REST API

### エンドポイント一覧

**センサー管理:**
- `GET /api/v1/sensors` - センサー一覧取得
- `GET /api/v1/sensors/{id}` - 個別センサー情報
- `PATCH /api/v1/sensors/{id}` - センサー設定更新

**フィルター設定:**
- `GET /api/v1/filters` - フィルター設定取得
- `PUT /api/v1/filters/prefilter` - プリフィルター更新
- `PUT /api/v1/filters/postfilter` - ポストフィルター更新

**設定管理:**
- `GET /api/v1/configs/list` - 保存済み設定一覧
- `POST /api/v1/configs/save` - 設定保存
- `POST /api/v1/configs/load` - 設定読み込み
- `GET /api/v1/configs/export` - 設定エクスポート
- `POST /api/v1/configs/import` - 設定インポート

**スナップショット:**
- `GET /api/v1/snapshot` - 現在の全設定取得

### API テスト

```bash
# テストスクリプト実行
./scripts/test_rest_api.sh http://localhost:8080
```

## 開発・デバッグ

### ログ出力
アプリケーションは標準出力にログを出力します：
- フレーム処理統計
- フィルター適用結果
- エラー・警告メッセージ

### パフォーマンス監視
WebUIでリアルタイム監視可能：
- FPS（フレームレート）
- 点群数（Raw/Filtered）
- クラスター数
- 接続状態

## システムサービス化

### systemd設定例

```ini
# /etc/systemd/system/hokuyo-hub.service
[Unit]
Description=HokuyoHub LiDAR Processing Service
After=network.target

[Service]
Type=simple
User=hokuyohub
WorkingDirectory=/opt/hokuyohub
ExecStart=/opt/hokuyohub/hokuyo_hub --config /etc/hokuyohub/config.yaml
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable hokuyo-hub
sudo systemctl start hokuyo-hub
```

## トラブルシューティング

### よくある問題

1. **センサー接続エラー**
   - ネットワーク設定確認
   - ファイアウォール設定確認
   - センサーIPアドレス確認

2. **WebUI接続できない**
   - ポート8080が使用可能か確認
   - `--listen` パラメータ確認

3. **パフォーマンス問題**
   - フィルター設定の調整
   - WebUIのパフォーマンスモード有効化（`P`キー）

### ログ確認

```bash
# アプリケーションログ
./hokuyo_hub --config config.yaml 2>&1 | tee hokuyo.log

# systemdログ
sudo journalctl -u hokuyo-hub -f
```

## ライセンス

このプロジェクトは開発中のため、ライセンスは未定です。

## 貢献

バグ報告や機能要望は、プロジェクトの課題管理システムを通じてお知らせください。
