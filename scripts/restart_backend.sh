#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== バックエンド再起動 ==="

# バックエンド停止
if [ -f "$PROJECT_ROOT/backend.pid" ]; then
    BACKEND_PID=$(cat "$PROJECT_ROOT/backend.pid")
    if kill "$BACKEND_PID" 2>/dev/null; then
        echo "既存バックエンド停止: PID $BACKEND_PID"
    fi
    rm -f "$PROJECT_ROOT/backend.pid"
fi

# 強制停止（念のため）
pkill -f "hokuyo_hub" 2>/dev/null || true

# 少し待機
sleep 1

# 新バックエンド起動
"$SCRIPT_DIR/start_backend.sh"

echo "✅ バックエンド再起動完了"