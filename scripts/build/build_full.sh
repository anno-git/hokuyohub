#!/bin/bash

echo "=== HokuyoHub分離アーキテクチャ ビルド ==="

PROJECT_ROOT="$(dirname "$(dirname "$(dirname "$(realpath "$0")")")")"
cd "$PROJECT_ROOT"

# 1. バックエンドビルド
echo "1. バックエンド (C++) ビルド中..."
if ! cmake -B build/darwin-arm64 -DCMAKE_BUILD_TYPE=Release; then
    echo "❌ CMake configuration failed"
    exit 1
fi

if ! cmake --build build/darwin-arm64 --parallel; then
    echo "❌ Backend build failed"
    exit 1
fi

# 2. WebUIサーバー準備
echo "2. WebUIサーバー (Node.js) セットアップ中..."
if [ -d "webui-server" ]; then
    cd webui-server
    if ! npm install; then
        echo "❌ WebUI server npm install failed"
        exit 1
    fi
    cd ..
else
    echo "❌ webui-server directory not found"
    exit 1
fi

# 3. dist ディレクトリ構成
echo "3. 配布パッケージ作成中..."
mkdir -p dist/darwin-arm64/{bin,webui-server,configs}

# バックエンドバイナリ配置
if [ -f "build/darwin-arm64/hokuyo_hub" ]; then
    cp build/darwin-arm64/hokuyo_hub dist/darwin-arm64/bin/
else
    echo "❌ Backend binary not found"
    exit 1
fi

# WebUIサーバー配置
cp -r webui-server/* dist/darwin-arm64/webui-server/

# 設定ファイル配置
if [ -d "configs" ]; then
    cp -r configs/* dist/darwin-arm64/configs/
fi

echo "✅ ビルド完了: dist/darwin-arm64/"
echo ""
echo "🚀 起動方法:"
echo "  統合起動: ./scripts/start_system.sh"
echo "  個別起動: ./scripts/start_backend.sh && ./scripts/start_webui.sh"