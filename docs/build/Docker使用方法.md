# HokuyoHub Docker 使用方法

**コンテナベースの開発・デプロイメント完全ガイド**

このガイドでは、HokuyoHubのDocker環境での開発、ビルド、デプロイメントについて詳しく説明します。CrowCpp移行により、Dockerイメージがより軽量で効率的になりました。

## 🚀 CrowCpp移行によるDocker改善

### コンテナの利点

**イメージサイズの削減:**
- **ヘッダーオンリー**: Webフレームワークのライブラリファイルが不要
- **スタティックリンク**: 動的ライブラリ依存関係を削減
- **最小依存関係**: Alpine Linuxベースで20MB以下のランタイムイメージ

**ビルド時間の改善:**
- **高速ビルド**: マルチステージビルドでキャッシュ効率向上
- **並列処理**: BuildKitによる並列依存関係解決
- **レイヤー最適化**: 変更頻度に基づくレイヤー設計

**運用性の向上:**
- **高速起動**: コンテナ起動時間の短縮
- **予測可能なリソース**: 一貫したメモリ・CPU使用量
- **簡単スケーリング**: マイクロサービス対応の軽量設計

## 🏗️ Docker イメージのビルド

### 基本的なビルド

**シンプルなビルド:**
```bash
# 現在のプラットフォーム用ビルド
docker build -t hokuyo-hub:latest .

# タグ付きビルド
docker build -t hokuyo-hub:v2.0.0 .

# ビルド引数を使用
docker build --build-arg BUILD_TYPE=Release -t hokuyo-hub:release .
```

**マルチプラットフォームビルド:**
```bash
# Docker Buildx の設定
docker buildx create --name hokuyo-builder --use
docker buildx inspect --bootstrap

# 複数プラットフォーム対応ビルド
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  --tag hokuyo-hub:latest \
  --push .

# ローカルでARM64イメージを作成（Apple Silicon）
docker buildx build \
  --platform linux/arm64 \
  --load \
  --tag hokuyo-hub:arm64 .
```

### 最適化されたDockerfile

**本番用マルチステージDockerfile:**
```dockerfile
# syntax=docker/dockerfile:1
FROM debian:bookworm-slim AS base
RUN apt-get update && apt-get install -y \
    ca-certificates \
    libyaml-cpp0.7 \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get clean

# ビルドステージ
FROM base AS builder
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    libyaml-cpp-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# ビルド実行
ARG BUILD_TYPE=Release
ARG JOBS=4
RUN cmake --preset linux-release \
    && cmake --build build/linux-arm64 --parallel ${JOBS} \
    && cmake --install build/linux-arm64

# ランタイムステージ
FROM base AS runtime

# 非rootユーザーの作成
RUN groupadd -r hokuyo && useradd -r -g hokuyo hokuyo

# アプリケーションファイルのコピー
COPY --from=builder /app/dist/linux-arm64/ /opt/hokuyo/
COPY configs/production.yaml /etc/hokuyo/

# 権限設定
RUN mkdir -p /var/log/hokuyo /opt/hokuyo/data \
    && chown -R hokuyo:hokuyo /var/log/hokuyo /opt/hokuyo/data \
    && chmod +x /opt/hokuyo/hokuyo_hub

# ヘルスチェックの追加
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
  CMD curl -f http://localhost:8081/health || exit 1

# 非rootユーザーに切り替え
USER hokuyo

# ポート露出
EXPOSE 8080 8081 9090

# ラベル（メタデータ）
LABEL org.opencontainers.image.title="HokuyoHub" \
      org.opencontainers.image.description="LiDAR processing application" \
      org.opencontainers.image.version="2.0.0" \
      org.opencontainers.image.vendor="HokuyoHub Project"

# エントリーポイント
ENTRYPOINT ["/opt/hokuyo/hokuyo_hub"]
CMD ["--config", "/etc/hokuyo/production.yaml"]
```

**開発用Dockerfile:**
```dockerfile
# docker/Dockerfile.dev
FROM ubuntu:22.04

# 開発ツールのインストール
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    gdb \
    valgrind \
    clang-format \
    clang-tidy \
    libyaml-cpp-dev \
    curl \
    wget \
    python3 \
    python3-pip \
    vim \
    && rm -rf /var/lib/apt/lists/*

# 追加開発ツール
RUN pip3 install --no-cache-dir conan pre-commit

# ccacheのインストール
RUN apt-get update && apt-get install -y ccache && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

# Git設定（開発コンテナ用）
RUN git config --global --add safe.directory /workspace

# エントリーポイント
CMD ["/bin/bash"]
```

### ビルド最適化

**ビルドキャッシュの活用:**
```bash
# キャッシュマウントを使用
docker buildx build \
  --cache-from type=local,src=/tmp/docker-cache \
  --cache-to type=local,dest=/tmp/docker-cache \
  -t hokuyo-hub:latest .

# レジストリキャッシュを使用
docker buildx build \
  --cache-from type=registry,ref=your-registry/hokuyo-hub:cache \
  --cache-to type=registry,ref=your-registry/hokuyo-hub:cache,mode=max \
  -t hokuyo-hub:latest .
```

**並列ビルドの最適化:**
```dockerfile
# Dockerfileでビルド並列度を制御
ARG JOBS=4
RUN cmake --build build/linux-arm64 --parallel ${JOBS}

# Docker Composeでビルド引数を設定
# docker-compose.yml
services:
  app:
    build:
      context: .
      args:
        JOBS: 8
        BUILD_TYPE: Release
```

## 🐳 Docker Compose 設定

### 開発環境用

**docker-compose.dev.yml:**
```yaml
version: '3.8'

services:
  hokuyo-hub:
    build:
      context: .
      dockerfile: docker/Dockerfile.dev
    container_name: hokuyo_hub_dev
    volumes:
      - .:/workspace
      - dev-build-cache:/workspace/build
      - ~/.gitconfig:/root/.gitconfig:ro
    working_dir: /workspace
    ports:
      - "8080:8080"
      - "8081:8081"
      - "9090:9090"
    environment:
      - DISPLAY=${DISPLAY:-:0}  # GUI アプリケーション用
      - CCACHE_DIR=/workspace/.ccache
    tty: true
    stdin_open: true
    networks:
      - dev-network

  # テスト用Redis（キャッシュ・セッション）
  redis:
    image: redis:7-alpine
    container_name: hokuyo_redis_dev
    ports:
      - "6379:6379"
    volumes:
      - redis-data:/data
    networks:
      - dev-network

  # 監視・メトリクス（開発用）
  prometheus:
    image: prom/prometheus:latest
    container_name: hokuyo_prometheus_dev
    ports:
      - "9091:9090"
    volumes:
      - ./monitoring/prometheus.dev.yml:/etc/prometheus/prometheus.yml
    command:
      - '--config.file=/etc/prometheus/prometheus.yml'
      - '--storage.tsdb.path=/prometheus'
      - '--web.console.libraries=/etc/prometheus/console_libraries'
      - '--web.console.templates=/etc/prometheus/consoles'
    networks:
      - dev-network

volumes:
  dev-build-cache:
  redis-data:

networks:
  dev-network:
    driver: bridge
```

### 本番環境用

**docker-compose.prod.yml:**
```yaml
version: '3.8'

services:
  hokuyo-hub:
    image: your-registry/hokuyo-hub:v2.0.0
    container_name: hokuyo_hub_prod
    restart: unless-stopped
    ports:
      - "8080:8080"
      - "8081:8081"
    volumes:
      - ./configs/production.yaml:/etc/hokuyo/production.yaml:ro
      - ./logs:/var/log/hokuyo
      - ./data:/opt/hokuyo/data
      - /etc/localtime:/etc/localtime:ro
    environment:
      - HOKUYO_CONFIG=/etc/hokuyo/production.yaml
      - HOKUYO_LOG_LEVEL=info
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8081/health"]
      interval: 30s
      timeout: 10s
      retries: 3
      start_period: 10s
    deploy:
      resources:
        limits:
          memory: 1G
          cpus: '2'
        reservations:
          memory: 512M
          cpus: '1'
    logging:
      driver: "json-file"
      options:
        max-size: "10m"
        max-file: "3"
    networks:
      - prod-network

  # リバースプロキシ（Nginx）
  nginx:
    image: nginx:alpine
    container_name: hokuyo_nginx_prod
    restart: unless-stopped
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./nginx/nginx.conf:/etc/nginx/nginx.conf:ro
      - ./nginx/ssl:/etc/nginx/ssl:ro
      - ./logs/nginx:/var/log/nginx
    depends_on:
      - hokuyo-hub
    networks:
      - prod-network

  # 監視スタック
  prometheus:
    image: prom/prometheus:latest
    container_name: hokuyo_prometheus_prod
    restart: unless-stopped
    ports:
      - "9091:9090"
    volumes:
      - ./monitoring/prometheus.yml:/etc/prometheus/prometheus.yml:ro
      - prometheus-data:/prometheus
    command:
      - '--config.file=/etc/prometheus/prometheus.yml'
      - '--storage.tsdb.path=/prometheus'
      - '--storage.tsdb.retention.time=30d'
    networks:
      - prod-network

  grafana:
    image: grafana/grafana:latest
    container_name: hokuyo_grafana_prod
    restart: unless-stopped
    ports:
      - "3000:3000"
    volumes:
      - grafana-data:/var/lib/grafana
      - ./monitoring/grafana/provisioning:/etc/grafana/provisioning:ro
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=${GRAFANA_PASSWORD:-admin}
    networks:
      - prod-network

volumes:
  prometheus-data:
  grafana-data:

networks:
  prod-network:
    driver: bridge
```

## 🚀 実行とデプロイメント

### 基本的な実行

**単体コンテナ実行:**
```bash
# シンプルな実行
docker run -d \
  --name hokuyo-hub \
  -p 8080:8080 \
  hokuyo-hub:latest

# 設定ファイルをマウント
docker run -d \
  --name hokuyo-hub \
  -p 8080:8080 \
  -v $(pwd)/configs/production.yaml:/etc/hokuyo/production.yaml:ro \
  -v $(pwd)/logs:/var/log/hokuyo \
  hokuyo-hub:latest

# 環境変数で設定
docker run -d \
  --name hokuyo-hub \
  -p 8080:8080 \
  -e HOKUYO_LOG_LEVEL=debug \
  -e HOKUYO_SENSOR_IP=192.168.1.10 \
  hokuyo-hub:latest
```

**Docker Compose実行:**
```bash
# 開発環境
docker-compose -f docker-compose.dev.yml up -d

# 本番環境
docker-compose -f docker-compose.prod.yml up -d

# ログ確認
docker-compose -f docker-compose.prod.yml logs -f hokuyo-hub

# 停止
docker-compose -f docker-compose.prod.yml down

# 完全削除（ボリューム含む）
docker-compose -f docker-compose.prod.yml down -v
```

### 開発ワークフロー

**開発コンテナでの作業:**
```bash
# 開発環境起動
docker-compose -f docker-compose.dev.yml up -d

# コンテナ内で作業
docker-compose -f docker-compose.dev.yml exec hokuyo-hub bash

# コンテナ内でビルド
cmake --preset linux-debug
cmake --build build/linux-arm64

# テスト実行
ctest --test-dir build/linux-arm64

# ファイル変更を監視（ホットリロード）
inotifywait -m -r -e modify,move,create,delete /workspace/src | while read; do
    echo "ファイル変更検出、再ビルド中..."
    cmake --build build/linux-arm64
done
```

**VSCodeでの開発:**
```json
// .devcontainer/devcontainer.json
{
    "name": "HokuyoHub Development",
    "dockerComposeFile": "../docker-compose.dev.yml",
    "service": "hokuyo-hub",
    "workspaceFolder": "/workspace",
    "extensions": [
        "ms-vscode.cpptools",
        "ms-vscode.cmake-tools",
        "vadimcn.vscode-lldb"
    ],
    "settings": {
        "cmake.configureOnOpen": true,
        "cmake.buildDirectory": "${workspaceFolder}/build/linux-arm64"
    },
    "forwardPorts": [8080, 8081, 9090],
    "postCreateCommand": "git submodule update --init --recursive"
}
```

## 🔍 監視と診断

### ヘルスチェック

**コンテナヘルスチェック:**
```bash
# ヘルスチェック状況確認
docker ps --format "table {{.Names}}\t{{.Status}}"

# 詳細ヘルスチェック情報
docker inspect hokuyo-hub | jq '.[0].State.Health'

# 手動ヘルスチェック
docker exec hokuyo-hub curl -f http://localhost:8081/health
```

**アプリケーションメトリクス:**
```bash
# メトリクス取得
curl http://localhost:9090/metrics

# 特定メトリクスのフィルタ
curl http://localhost:9090/metrics | grep hokuyo_

# Prometheusクエリ（PromQLの例）
curl 'http://localhost:9091/api/v1/query?query=hokuyo_sensor_count'
```

### ログ管理

**ログ確認:**
```bash
# リアルタイムログ
docker logs -f hokuyo-hub

# 特定期間のログ
docker logs --since "2025-01-01T00:00:00" --until "2025-01-01T23:59:59" hokuyo-hub

# ログレベルでフィルタ
docker logs hokuyo-hub 2>&1 | grep -E "(ERROR|WARN)"

# JSON形式ログの解析
docker logs hokuyo-hub 2>&1 | jq '.level, .message'
```

**ログローテーション:**
```json
// Docker daemon.json
{
    "log-driver": "json-file",
    "log-opts": {
        "max-size": "10m",
        "max-file": "3"
    }
}
```

### パフォーマンス監視

**リソース使用量:**
```bash
# リアルタイムリソース監視
docker stats hokuyo-hub

# 全コンテナの統計
docker stats --format "table {{.Container}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.NetIO}}"

# メモリ使用量詳細
docker exec hokuyo-hub cat /proc/meminfo

# CPU 使用量詳細
docker exec hokuyo-hub cat /proc/loadavg
```

**プロファイリング:**
```bash
# コンテナ内でのプロファイリング
docker exec -it hokuyo-hub bash
perf record -g -p $(pgrep hokuyo_hub)
perf report

# cAdvisor で監視（推奨）
docker run \
  --volume=/:/rootfs:ro \
  --volume=/var/run:/var/run:ro \
  --volume=/sys:/sys:ro \
  --volume=/var/lib/docker/:/var/lib/docker:ro \
  --publish=8080:8080 \
  --detach=true \
  --name=cadvisor \
  gcr.io/cadvisor/cadvisor:latest
```

## 🔧 トラブルシューティング

### 一般的な問題

**コンテナが起動しない:**
```bash
# エラーログ確認
docker logs hokuyo-hub

# イメージの検査
docker inspect hokuyo-hub:latest

# コンテナの詳細状態
docker inspect hokuyo-hub

# 手動でコンテナ内に入る
docker run -it --entrypoint /bin/bash hokuyo-hub:latest
```

**ポート競合:**
```bash
# ポート使用状況確認
netstat -tlnp | grep :8080
lsof -i :8080

# 異なるポートで起動
docker run -d -p 8081:8080 hokuyo-hub:latest

# Docker Composeでポート変更
# docker-compose.yml
ports:
  - "8081:8080"  # ホスト:コンテナ
```

**ボリューム・マウント問題:**
```bash
# マウント状況確認
docker inspect hokuyo-hub | jq '.[0].Mounts'

# 権限問題の解決
docker exec hokuyo-hub chown -R hokuyo:hokuyo /var/log/hokuyo

# ボリューム一覧
docker volume ls

# ボリューム詳細
docker volume inspect hokuyo_logs
```

### ネットワーク問題

**コンテナ間通信:**
```bash
# ネットワーク一覧
docker network ls

# ネットワーク詳細
docker network inspect bridge

# コンテナのIPアドレス確認
docker inspect hokuyo-hub | jq '.[0].NetworkSettings.IPAddress'

# ping テスト
docker exec hokuyo-hub ping redis
```

**DNS解決:**
```bash
# DNS設定確認
docker exec hokuyo-hub cat /etc/resolv.conf

# 外部接続テスト
docker exec hokuyo-hub nslookup google.com

# サービスディスカバリテスト
docker exec hokuyo-hub curl http://redis:6379
```

## 📦 イメージ管理

### レジストリ操作

**イメージのプッシュ・プル:**
```bash
# ローカルイメージ一覧
docker images hokuyo-hub

# タグ付け
docker tag hokuyo-hub:latest your-registry/hokuyo-hub:v2.0.0

# プッシュ
docker push your-registry/hokuyo-hub:v2.0.0

# プル
docker pull your-registry/hokuyo-hub:v2.0.0

# マルチプラットフォームプッシュ
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  --tag your-registry/hokuyo-hub:latest \
  --push .
```

**イメージクリーンアップ:**
```bash
# 未使用イメージ削除
docker image prune

# 全未使用オブジェクト削除
docker system prune -a

# 特定期間以前のイメージ削除
docker image prune --filter "until=24h"

# ビルドキャッシュクリア
docker builder prune
```

## 🚀 CI/CD統合

### GitHub Actions でのDocker操作

**ビルドとプッシュ:**
```yaml
# .github/workflows/docker.yml
name: Docker Build and Push

on:
  push:
    branches: [main]
    tags: ['v*']

jobs:
  docker:
    runs-on: ubuntu-latest
    steps:
    - name: Checkout
      uses: actions/checkout@v4
      with:
        submodules: recursive

    - name: Set up Docker Buildx
      uses: docker/setup-buildx-action@v3

    - name: Login to Registry
      uses: docker/login-action@v3
      with:
        registry: your-registry.com
        username: ${{ secrets.REGISTRY_USERNAME }}
        password: ${{ secrets.REGISTRY_PASSWORD }}

    - name: Extract metadata
      id: meta
      uses: docker/metadata-action@v5
      with:
        images: your-registry/hokuyo-hub
        tags: |
          type=ref,event=branch
          type=ref,event=pr
          type=semver,pattern={{version}}
          type=semver,pattern={{major}}.{{minor}}

    - name: Build and push
      uses: docker/build-push-action@v5
      with:
        context: .
        platforms: linux/amd64,linux/arm64
        push: true
        tags: ${{ steps.meta.outputs.tags }}
        labels: ${{ steps.meta.outputs.labels }}
        cache-from: type=gha
        cache-to: type=gha,mode=max
```

---

**Docker使用方法 バージョン:** 2.0  
**最終更新:** 2025-01-01  
**対象:** 開発、本番、CI/CD、監視、トラブルシューティング  
**プラットフォーム:** linux/amd64, linux/arm64 マルチプラットフォーム対応