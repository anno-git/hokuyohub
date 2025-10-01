#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DIST_DIR="$PROJECT_ROOT/dist/darwin-arm64"

echo "=== バックエンド起動 ==="

# 事前チェック
if [ ! -f "$DIST_DIR/bin/hokuyo_hub" ]; then
    echo "❌ バックエンドバイナリが見つかりません: $DIST_DIR/bin/hokuyo_hub"
    echo "💡 先にビルドを実行してください: ./scripts/build/build_backend_only.sh"
    exit 1
fi

# 既存プロセス終了
pkill -f "hokuyo_hub" 2>/dev/null || true
rm -f "$PROJECT_ROOT/backend.pid"

# バックエンド起動
cd "$DIST_DIR"
./bin/hokuyo_hub --listen 0.0.0.0:8081 --config configs/default.yaml &
BACKEND_PID=$!

# 起動確認
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
echo "バックエンドPID: $BACKEND_PID"
echo "Backend API: http://localhost:8081"