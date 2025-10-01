#!/bin/bash
# HokuyoHub ヘルスチェックスクリプト

WEBUI_URL="http://localhost:8080"
BACKEND_URL="http://localhost:8081"
TIMEOUT=10

echo "=== HokuyoHub Health Check ==="
echo "Timestamp: $(date)"

# WebUI サーバーチェック
echo -n "WebUI Server (8080): "
if curl -s --max-time $TIMEOUT "$WEBUI_URL" > /dev/null; then
    echo "✓ OK"
    WEBUI_STATUS=0
else
    echo "✗ FAILED"
    WEBUI_STATUS=1
fi

# Backend API チェック
echo -n "Backend API (8081): "
if curl -s --max-time $TIMEOUT "$BACKEND_URL/api/v1/health" > /dev/null; then
    echo "✓ OK"
    BACKEND_STATUS=0
else
    echo "✗ FAILED"  
    BACKEND_STATUS=1
fi

# プロセスチェック
echo -n "Backend Process: "
if pgrep -f "hokuyo_hub" > /dev/null; then
    PID=$(pgrep -f "hokuyo_hub")
    echo "✓ Running (PID: $PID)"
    PROCESS_STATUS=0
else
    echo "✗ Not Found"
    PROCESS_STATUS=1
fi

echo -n "WebUI Process: "
if pgrep -f "node.*server\.js" > /dev/null; then
    PID=$(pgrep -f "node.*server\.js")
    echo "✓ Running (PID: $PID)"
    WEBUI_PROCESS_STATUS=0
else
    echo "✗ Not Found"
    WEBUI_PROCESS_STATUS=1
fi

# 総合判定
TOTAL_ISSUES=$((WEBUI_STATUS + BACKEND_STATUS + PROCESS_STATUS + WEBUI_PROCESS_STATUS))

echo ""
if [ $TOTAL_ISSUES -eq 0 ]; then
    echo "🟢 System Status: HEALTHY"
    exit 0
else
    echo "🔴 System Status: ISSUES DETECTED ($TOTAL_ISSUES issues)"
    
    # 推奨対処方法
    echo ""
    echo "Recommended actions:"
    if [ $WEBUI_STATUS -ne 0 ] || [ $WEBUI_PROCESS_STATUS -ne 0 ]; then
        echo "- Check WebUI server: systemctl status hokuyo_hub"
    fi
    if [ $BACKEND_STATUS -ne 0 ] || [ $PROCESS_STATUS -ne 0 ]; then
        echo "- Restart backend: curl -X POST http://localhost:8080/api/v1/backend/restart"
    fi
    exit 1
fi