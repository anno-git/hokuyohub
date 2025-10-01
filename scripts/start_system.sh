#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DIST_DIR="$PROJECT_ROOT/dist/darwin-arm64"

echo "=== HokuyoHub分離システム起動 ==="

# 事前チェック: 必要なファイルの存在確認
if [ ! -f "$DIST_DIR/bin/hokuyo_hub" ]; then
    echo "❌ バックエンドバイナリが見つかりません: $DIST_DIR/bin/hokuyo_hub"
    echo "💡 先にビルドを実行してください: ./scripts/build/build_full.sh"
    exit 1
fi

if [ ! -f "$DIST_DIR/webui-server/package.json" ]; then
    echo "❌ WebUIサーバーが見つかりません: $DIST_DIR/webui-server/"
    echo "💡 先にビルドを実行してください: ./scripts/build/build_full.sh"
    exit 1
fi

# 既存プロセス終了
echo "既存プロセス停止中..."
pkill -f "hokuyo_hub" 2>/dev/null || true
pkill -f "node.*webui-server" 2>/dev/null || true

# PIDファイルクリーンアップ
rm -f "$PROJECT_ROOT/backend.pid" "$PROJECT_ROOT/webui.pid"

# 1. バックエンド起動 (ポート8081)
echo "1. バックエンド起動中..."
cd "$DIST_DIR"
./bin/hokuyo_hub --listen 0.0.0.0:8081 --config configs/default.yaml &
BACKEND_PID=$!
echo "バックエンドPID: $BACKEND_PID"

# 2. バックエンド起動待機
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

# 3. WebUIサーバー起動 (ポート8080)
echo "2. WebUIサーバー起動中..."
cd "$DIST_DIR/webui-server"
npm start &
WEBUI_PID=$!
echo "WebUIサーバーPID: $WEBUI_PID"

# 4. WebUIサーバー起動待機
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

# 5. PIDファイル保存
echo "$BACKEND_PID" > "$PROJECT_ROOT/backend.pid"
echo "$WEBUI_PID" > "$PROJECT_ROOT/webui.pid"

echo ""
echo "✅ システム起動完了"
echo "WebUI: http://localhost:8080"
echo "Backend API: http://localhost:8081"
echo ""
echo "停止するには: ./scripts/stop_system.sh"