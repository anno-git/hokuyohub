#!/bin/bash
# パフォーマンステストスイート

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PERF_LOG="/tmp/hokuyo_hub_performance_$(date +%Y%m%d_%H%M%S).log"

echo "=== HokuyoHub Performance Test ===" | tee "$PERF_LOG"
echo "Timestamp: $(date)" | tee -a "$PERF_LOG"

# システム起動
cd "$PROJECT_ROOT"
echo "Starting system for performance testing..." | tee -a "$PERF_LOG"
./scripts/start_system.sh &

# 起動待機
echo "Waiting for system startup..." | tee -a "$PERF_LOG"
sleep 15

# API レスポンス時間測定
echo "=== API Response Time Test ===" | tee -a "$PERF_LOG"

for i in {1..10}; do
    time_total=$(curl -w "%{time_total}" -s "http://localhost:8080/api/v1/sensors" -o /dev/null)
    echo "API Request $i: ${time_total}s" | tee -a "$PERF_LOG"
done

# 同時接続テスト
echo "=== Concurrent Connection Test ===" | tee -a "$PERF_LOG"

for concurrent in 5 10 20; do
    echo "Testing $concurrent concurrent connections..." | tee -a "$PERF_LOG"
    
    start_time=$(date +%s.%N)
    
    for i in $(seq 1 $concurrent); do
        curl -s "http://localhost:8080/" > /dev/null &
    done
    wait
    
    end_time=$(date +%s.%N)
    duration=$(echo "$end_time - $start_time" | bc)
    echo "$concurrent connections completed in: ${duration}s" | tee -a "$PERF_LOG"
done

# メモリ使用量追跡
echo "=== Memory Usage Tracking ===" | tee -a "$PERF_LOG"

for i in {1..5}; do
    backend_pid=$(pgrep -f "hokuyo_hub")
    webui_pid=$(pgrep -f "webui-server")
    
    backend_mem=$(ps -p $backend_pid -o rss= 2>/dev/null || echo "0")
    webui_mem=$(ps -p $webui_pid -o rss= 2>/dev/null || echo "0")
    
    echo "Sample $i - Backend: ${backend_mem}KB, WebUI: ${webui_mem}KB" | tee -a "$PERF_LOG"
    sleep 5
done

# システム停止
./scripts/stop_system.sh

echo "Performance test completed. Log: $PERF_LOG" | tee -a "$PERF_LOG"