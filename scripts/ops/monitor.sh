#!/bin/bash
# リアルタイムモニタリングスクリプト

INTERVAL="${1:-5}"

echo "=== HokuyoHub Monitor (${INTERVAL}s interval) ==="
echo "Press Ctrl+C to stop"

while true; do
    clear
    echo "=== HokuyoHub Status - $(date) ==="
    echo ""
    
    # サービス状態
    ./health_check.sh 2>/dev/null
    echo ""
    
    # リソース使用状況
    echo "=== Resource Usage ==="
    if pgrep -f "hokuyo_hub" > /dev/null; then
        PID=$(pgrep -f "hokuyo_hub")
        echo "Backend CPU/Memory:"
        ps -p $PID -o pid,pcpu,pmem,rss,vsz,comm 2>/dev/null || echo "Process not found"
    fi
    
    if pgrep -f "webui-server" > /dev/null; then
        PID=$(pgrep -f "webui-server")
        echo "WebUI CPU/Memory:"
        ps -p $PID -o pid,pcpu,pmem,rss,vsz,comm 2>/dev/null || echo "Process not found"
    fi
    
    echo ""
    echo "System Load: $(uptime | awk '{print $10,$11,$12}')"
    echo "Memory: $(free -h | grep Mem | awk '{print $3"/"$2" ("$3*100/$2"%)"}')"
    
    sleep $INTERVAL
done