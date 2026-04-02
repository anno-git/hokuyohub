# HokuyoHub Quick Start Guide

**Get HokuyoHub up and running in minutes.**
詳細なビルド手順は [BUILD.md](BUILD.md)、設定・操作の詳細は [README.md](README.md) を参照してください。

## macOS (開発向け)

```bash
# 前提: Homebrew, CMake
brew install cmake

# クローン & ビルド
git clone <repository-url>
cd HokuyoHub/proj
./scripts/build/build_with_presets.sh release --install

# バックエンド起動
./dist/darwin-arm64/hokuyo_hub --config ./configs/default.yaml --listen 0.0.0.0:8081

# フロントエンド起動 (別ターミナル)
cd webui-server && npm start
```

**Open:** http://localhost:3000

## Raspberry Pi 5 (本番向け)

**macOS でクロスコンパイル:**
```bash
./scripts/setup/setup_cross_compile.sh   # 初回のみ
./scripts/build/docker_cross_build.sh --build-all
scp -r dist/linux-arm64/ pi@raspberrypi:~/hokuyo-hub/
```

**Raspberry Pi で実行:**
```bash
sudo apt update && sudo apt install libyaml-cpp0.7 libnng1 libssl3 zlib1g
cd ~/hokuyo-hub && chmod +x hokuyo_hub
./hokuyo_hub --config ./configs/default.yaml --listen 0.0.0.0:8081
```

**Open:** http://raspberrypi.local:8081

## Docker

```bash
./docker/build.sh build-all
./scripts/utils/extract_docker_artifacts.sh hokuyo-hub:latest

# ローカル実行
cd dist/linux-arm64
./hokuyo_hub --config ./configs/default.yaml --listen 0.0.0.0:8081

# またはコンテナ実行
docker run -d -p 8081:8081 --name hokuyo-hub hokuyo-hub:latest
```

**Open:** http://localhost:8081

## 初回セットアップ

### 1. センサー設定

`configs/default.yaml` を編集:

```yaml
sensors:
  - id: "sensor1"
    name: "front-lidar"
    type: "hokuyo_urg_eth"
    endpoint: "192.168.1.100:10940"  # センサーのIPに変更
    pose: { tx: 0.0, ty: 0.0, theta: 0.0 }
    enabled: true
```

### 2. ネットワーク設定 (Ethernetセンサーの場合)

```bash
sudo ip addr add 192.168.1.10/24 dev eth0
sudo ip link set eth0 up
ping 192.168.1.100  # センサーのIPに変更
```

### 3. Web UIにアクセス

1. ブラウザで http://localhost:3000 (フロントエンド) または http://localhost:8081 (バックエンド直接) を開く
2. **"Add Sensor"** → センサー情報を入力 → **"Save"**
3. 中央のパネルでリアルタイムデータを確認

Web UIの詳しい操作方法は [README.md](README.md#-basic-usage) を参照してください。

## 次のステップ

- **[README.md](README.md)** — 機能詳細、設定リファレンス、REST API、メッセージフォーマット
- **[BUILD.md](BUILD.md)** — 全プラットフォームのビルド手順
- **[README-DEVELOPERS.md](README-DEVELOPERS.md)** — アーキテクチャ、コード構造、開発ガイド
- **[docs/build/troubleshooting.md](docs/build/troubleshooting.md)** — トラブルシューティング
