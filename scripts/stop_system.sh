#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== システム停止中 ==="

# PIDファイルから停止
if [ -f "$PROJECT_ROOT/backend.pid" ]; then
    BACKEND_PID=$(cat "$PROJECT_ROOT/backend.pid")
    if kill "$BACKEND_PID" 2>/dev/null; then
        echo "バックエンド停止: PID $BACKEND_PID"
    fi
    rm -f "$PROJECT_ROOT/backend.pid"
fi

if [ -f "$PROJECT_ROOT/webui.pid" ]; then
    WEBUI_PID=$(cat "$PROJECT_ROOT/webui.pid")
    if kill "$WEBUI_PID" 2>/dev/null; then
        echo "WebUIサーバー停止: PID $WEBUI_PID"
    fi
    rm -f "$PROJECT_ROOT/webui.pid"
fi

# プロセス名での強制停止（念のため）
if pkill -f "hokuyo_hub" 2>/dev/null; then
    echo "残存バックエンドプロセス強制停止"
fi

if pkill -f "node.*webui-server" 2>/dev/null; then
    echo "残存WebUIサーバープロセス強制停止"
fi

# 停止確認
sleep 1
BACKEND_RUNNING=$(pgrep -f "hokuyo_hub" | wc -l)
WEBUI_RUNNING=$(pgrep -f "node.*webui-server" | wc -l)

if [ $BACKEND_RUNNING -eq 0 ] && [ $WEBUI_RUNNING -eq 0 ]; then
    echo "✅ システム停止完了"
else
    echo "⚠️  一部プロセスが残存している可能性があります"
    echo "   バックエンド: $BACKEND_RUNNING プロセス"
    echo "   WebUIサーバー: $WEBUI_RUNNING プロセス"
fi