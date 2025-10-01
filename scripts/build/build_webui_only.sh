#!/bin/bash

echo "=== WebUIサーバーのみビルド ==="

PROJECT_ROOT="$(dirname "$(dirname "$(dirname "$(realpath "$0")")")")"
cd "$PROJECT_ROOT"

# WebUIサーバーセットアップ
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

# distディレクトリ作成とファイル配置
mkdir -p dist/darwin-arm64/webui-server
cp -r webui-server/* dist/darwin-arm64/webui-server/

echo "✅ WebUIサーバービルド完了"
echo "WebUIサーバー: dist/darwin-arm64/webui-server/"