#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DIST_DIR="$PROJECT_ROOT/dist/darwin-arm64"

echo "=== WebUIサーバー起動 ==="

# 事前チェック
if [ ! -f "$DIST_DIR/webui-server/package.json" ]; then
    echo "❌ WebUIサーバーが見つかりません: $DIST_DIR/webui-server/"
    echo "💡 先にビルドを実行してください: ./scripts/build/build_webui_only.sh"
    exit 1
fi

# 既存プロセス終了
pkill -f "node.*webui-server" 2>/dev/null || true
rm -f "$PROJECT_ROOT/webui.pid"

# WebUIサーバー起動
cd "$DIST_DIR/webui-server"
npm start &
WEBUI_PID=$!

# 起動確認
echo "WebUIサーバー起動待機中..."
for i in {1..10}; do
    if curl -s http://localhost:8080 > /dev/null 2>&1; then
        echo "✅ WebUIサーバー起動完了"
        break
    fi
    if [ $i -eq 10 ]; then
        echo "❌ WebUIサーバーの起動がタイムアウトしました"
        kill $WEBUI_PID 2>/dev/null || true
        exit 1
    fi
    sleep 1
done

echo "$WEBUI_PID" > "$PROJECT_ROOT/webui.pid"
echo "WebUIサーバーPID: $WEBUI_PID"
echo "WebUI: http://localhost:8080"