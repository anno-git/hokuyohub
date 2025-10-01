#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

echo "=== 開発モード起動 ==="

# Node.js環境チェック
if ! command -v node > /dev/null 2>&1; then
    echo "❌ Node.js が見つかりません"
    echo "💡 Node.js をインストールしてください"
    exit 1
fi

if ! command -v npm > /dev/null 2>&1; then
    echo "❌ npm が見つかりません"
    echo "💡 npm をインストールしてください"
    exit 1
fi

# バックエンドビルド（開発モード用）
echo "1. バックエンドビルド中..."
cd "$PROJECT_ROOT"
if ! cmake -B build/darwin-arm64 -DCMAKE_BUILD_TYPE=Debug; then
    echo "❌ CMake configuration failed"
    exit 1
fi

if ! cmake --build build/darwin-arm64 --parallel; then
    echo "❌ Backend build failed"
    exit 1
fi

# バックエンド起動
echo "2. バックエンド起動中..."
pkill -f "hokuyo_hub" 2>/dev/null || true
rm -f "$PROJECT_ROOT/backend.pid"

cd "$PROJECT_ROOT"
./build/darwin-arm64/hokuyo_hub --listen 0.0.0.0:8081 --config ./configs/default.yaml &
BACKEND_PID=$!

# バックエンド起動待機
echo "バックエンド起動待機中..."
for i in {1..10}; do
    if curl -s http://localhost:8081/api/v1/health > /dev/null 2>&1; then
        echo "✅ バックエンド起動完了"
        break
    fi
    if [ $i -eq 10 ]; then
        echo "❌ バックエンドの起動がタイムアウトしました"
        kill $BACKEND_PID 2>/dev/null || true
        exit 1
    fi
    sleep 1
done

echo "$BACKEND_PID" > "$PROJECT_ROOT/backend.pid"

# WebUIサーバー開発モード起動
echo "3. WebUIサーバー開発モード起動中..."
pkill -f "node.*webui-server" 2>/dev/null || true
rm -f "$PROJECT_ROOT/webui.pid"

if [ -d "$PROJECT_ROOT/webui-server" ]; then
    cd "$PROJECT_ROOT/webui-server"
    
    # 依存関係インストール（必要に応じて）
    if [ ! -d "node_modules" ]; then
        echo "依存関係インストール中..."
        npm install
    fi
    
    # 開発モード起動
    npm run dev &
    WEBUI_PID=$!
    
    # WebUIサーバー起動待機
    echo "WebUIサーバー起動待機中..."
    for i in {1..10}; do
        if curl -s http://localhost:8080 > /dev/null 2>&1; then
            echo "✅ WebUIサーバー起動完了"
            break
        fi
        if [ $i -eq 10 ]; then
            echo "❌ WebUIサーバーの起動がタイムアウトしました"
            kill $WEBUI_PID 2>/dev/null || true
            kill $BACKEND_PID 2>/dev/null || true
            exit 1
        fi
        sleep 1
    done
    
    echo "$WEBUI_PID" > "$PROJECT_ROOT/webui.pid"
else
    echo "❌ webui-server directory not found"
    kill $BACKEND_PID 2>/dev/null || true
    exit 1
fi

echo ""
echo "✅ 開発モード起動完了"
echo "WebUI Dev: http://localhost:8080 (ホットリロード有効)"
echo "Backend API: http://localhost:8081"
echo ""
echo "停止するには: ../stop_system.sh"