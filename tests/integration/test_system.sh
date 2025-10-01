#!/bin/bash
# HokuyoHub 統合テストスイート

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TEST_LOG="/tmp/hokuyo_hub_integration_test_$(date +%Y%m%d_%H%M%S).log"

echo "=== HokuyoHub Integration Test Suite ===" | tee "$TEST_LOG"
echo "Timestamp: $(date)" | tee -a "$TEST_LOG"
echo "Project Root: $PROJECT_ROOT" | tee -a "$TEST_LOG"
echo ""

# テスト結果追跡
TESTS_PASSED=0
TESTS_FAILED=0
FAILED_TESTS=()

# テスト関数
run_test() {
    local test_name="$1"
    local test_function="$2"
    
    echo -n "Testing: $test_name ... " | tee -a "$TEST_LOG"
    
    if $test_function >> "$TEST_LOG" 2>&1; then
        echo "✓ PASS" | tee -a "$TEST_LOG"
        ((TESTS_PASSED++))
        return 0
    else
        echo "✗ FAIL" | tee -a "$TEST_LOG"
        ((TESTS_FAILED++))
        FAILED_TESTS+=("$test_name")
        return 1
    fi
}

# Test 1: ビルド検証
test_build_integrity() {
    echo "--- Build Integrity Test ---"
    
    # バイナリ存在確認
    if [ ! -f "$PROJECT_ROOT/build/darwin-arm64/hokuyo_hub" ]; then
        echo "ERROR: Backend binary not found"
        return 1
    fi
    
    # WebUI server 存在確認
    if [ ! -f "$PROJECT_ROOT/webui-server/server.js" ]; then
        echo "ERROR: WebUI server not found"
        return 1
    fi
    
    # 設定ファイル確認
    if [ ! -f "$PROJECT_ROOT/configs/default.yaml" ]; then
        echo "ERROR: Default config not found"
        return 1
    fi
    
    echo "All required files present"
    return 0
}

# Test 2: システム起動・停止
test_system_lifecycle() {
    echo "--- System Lifecycle Test ---"
    
    # システム起動
    cd "$PROJECT_ROOT"
    timeout 30s ./scripts/start_system.sh &
    STARTUP_PID=$!
    
    # 起動待機
    local attempts=0
    local max_attempts=15
    
    while [ $attempts -lt $max_attempts ]; do
        if curl -s --max-time 5 "http://localhost:8080" > /dev/null && \
           curl -s --max-time 5 "http://localhost:8081/api/v1/health" > /dev/null; then
            echo "System startup successful"
            break
        fi
        
        sleep 2
        ((attempts++))
        
        if [ $attempts -eq $max_attempts ]; then
            echo "ERROR: System failed to start within timeout"
            kill $STARTUP_PID 2>/dev/null || true
            return 1
        fi
    done
    
    # プロセス確認
    if ! pgrep -f "hokuyo_hub" > /dev/null; then
        echo "ERROR: Backend process not running"
        return 1
    fi
    
    if ! pgrep -f "webui-server" > /dev/null; then
        echo "ERROR: WebUI process not running"
        return 1
    fi
    
    # システム停止
    ./scripts/stop_system.sh
    
    # 停止確認
    sleep 3
    if pgrep -f "hokuyo_hub" > /dev/null || pgrep -f "webui-server" > /dev/null; then
        echo "ERROR: Processes not properly terminated"
        return 1
    fi
    
    echo "System lifecycle test completed successfully"
    return 0
}

# Test 3: WebUI 機能テスト
test_webui_functionality() {
    echo "--- WebUI Functionality Test ---"
    
    # システム起動（背景）
    cd "$PROJECT_ROOT"
    ./scripts/start_system.sh &
    STARTUP_PID=$!
    
    # 起動待機
    local attempts=0
    while [ $attempts -lt 15 ]; do
        if curl -s "http://localhost:8080" > /dev/null; then
            break
        fi
        sleep 2
        ((attempts++))
    done
    
    if [ $attempts -eq 15 ]; then
        echo "ERROR: WebUI failed to start"
        kill $STARTUP_PID 2>/dev/null || true
        return 1
    fi
    
    # WebUI レスポンステスト
    local response=$(curl -s -w "%{http_code}" "http://localhost:8080" -o /dev/null)
    if [ "$response" != "200" ]; then
        echo "ERROR: WebUI returned HTTP $response"
        ./scripts/stop_system.sh
        return 1
    fi
    
    # バックエンドステータスAPI テスト
    local status_response=$(curl -s "http://localhost:8080/api/v1/backend/status")
    if ! echo "$status_response" | grep -q "status"; then
        echo "ERROR: Backend status API failed"
        ./scripts/stop_system.sh
        return 1
    fi
    
    ./scripts/stop_system.sh
    echo "WebUI functionality test passed"
    return 0
}

# Test 4: バックエンド再起動機能
test_backend_restart() {
    echo "--- Backend Restart Test ---"
    
    # システム起動
    cd "$PROJECT_ROOT"
    ./scripts/start_system.sh &
    
    # 起動待機
    local attempts=0
    while [ $attempts -lt 15 ]; do
        if curl -s "http://localhost:8081/api/v1/health" > /dev/null; then
            break
        fi
        sleep 2
        ((attempts++))
    done
    
    if [ $attempts -eq 15 ]; then
        echo "ERROR: System failed to start for restart test"
        return 1
    fi
    
    # 元のPID取得
    local old_pid=$(pgrep -f "hokuyo_hub")
    
    # 再起動実行
    local restart_response=$(curl -s -X POST "http://localhost:8080/api/v1/backend/restart")
    
    # 再起動確認待機
    sleep 5
    
    # 新しいPID確認
    local new_pid=$(pgrep -f "hokuyo_hub")
    
    if [ "$old_pid" = "$new_pid" ]; then
        echo "ERROR: Backend PID did not change ($old_pid -> $new_pid)"
        ./scripts/stop_system.sh
        return 1
    fi
    
    # ヘルスチェック
    if ! curl -s --max-time 10 "http://localhost:8081/api/v1/health" > /dev/null; then
        echo "ERROR: Backend not healthy after restart"
        ./scripts/stop_system.sh
        return 1
    fi
    
    ./scripts/stop_system.sh
    echo "Backend restart test passed (PID: $old_pid -> $new_pid)"
    return 0
}

# Test 5: 運用スクリプト検証
test_operation_scripts() {
    echo "--- Operation Scripts Test ---"
    
    # ヘルスチェックスクリプト実行可能性
    if [ ! -x "$PROJECT_ROOT/scripts/ops/health_check.sh" ]; then
        echo "ERROR: Health check script not executable"
        return 1
    fi
    
    # システム起動してヘルスチェック実行
    cd "$PROJECT_ROOT"
    ./scripts/start_system.sh &
    
    # 起動待機
    sleep 10
    
    # ヘルスチェック実行
    cd scripts/ops
    if ! ./health_check.sh > /dev/null; then
        echo "ERROR: Health check failed"
        cd "$PROJECT_ROOT"
        ./scripts/stop_system.sh
        return 1
    fi
    
    cd "$PROJECT_ROOT"
    ./scripts/stop_system.sh
    echo "Operation scripts test passed"
    return 0
}

# Test 6: パフォーマンス基本検証
test_performance_basic() {
    echo "--- Performance Basic Test ---"
    
    # システム起動
    cd "$PROJECT_ROOT"
    ./scripts/start_system.sh &
    
    # 起動待機
    sleep 10
    
    # メモリ使用量チェック
    local backend_pid=$(pgrep -f "hokuyo_hub")
    local webui_pid=$(pgrep -f "webui-server")
    
    local backend_mem=$(ps -p $backend_pid -o rss= 2>/dev/null || echo "0")
    local webui_mem=$(ps -p $webui_pid -o rss= 2>/dev/null || echo "0")
    
    # メモリ使用量閾値チェック（KB）
    local backend_limit=100000  # 100MB
    local webui_limit=150000    # 150MB
    
    if [ "$backend_mem" -gt "$backend_limit" ]; then
        echo "WARNING: Backend memory usage high: ${backend_mem}KB > ${backend_limit}KB"
    fi
    
    if [ "$webui_mem" -gt "$webui_limit" ]; then
        echo "WARNING: WebUI memory usage high: ${webui_mem}KB > ${webui_limit}KB"
    fi
    
    # レスポンス時間測定
    local response_time=$(curl -w "%{time_total}" -s "http://localhost:8080" -o /dev/null)
    echo "WebUI response time: ${response_time}s"
    
    ./scripts/stop_system.sh
    echo "Performance test completed (Backend: ${backend_mem}KB, WebUI: ${webui_mem}KB)"
    return 0
}

# Test 7: エラーハンドリング検証
test_error_handling() {
    echo "--- Error Handling Test ---"
    
    # 不正な設定でバックエンド起動テスト
    cd "$PROJECT_ROOT"
    
    # 不正ポート設定
    if timeout 10s ./build/darwin-arm64/hokuyo_hub --listen "invalid:port" 2>/dev/null; then
        echo "ERROR: Backend should fail with invalid port"
        return 1
    fi
    
    # 存在しない設定ファイル
    if timeout 10s ./build/darwin-arm64/hokuyo_hub --config "/nonexistent/config.yaml" 2>/dev/null; then
        echo "ERROR: Backend should fail with nonexistent config"
        return 1
    fi
    
    echo "Error handling test passed"
    return 0
}

# テスト実行
echo "Starting integration tests..." | tee -a "$TEST_LOG"
echo "===========================================" | tee -a "$TEST_LOG"

run_test "Build Integrity" test_build_integrity
run_test "System Lifecycle" test_system_lifecycle
run_test "WebUI Functionality" test_webui_functionality
run_test "Backend Restart" test_backend_restart
run_test "Operation Scripts" test_operation_scripts
run_test "Performance Basic" test_performance_basic
run_test "Error Handling" test_error_handling

# 最終結果
echo "" | tee -a "$TEST_LOG"
echo "===========================================" | tee -a "$TEST_LOG"
echo "Test Results:" | tee -a "$TEST_LOG"
echo "  Passed: $TESTS_PASSED" | tee -a "$TEST_LOG"
echo "  Failed: $TESTS_FAILED" | tee -a "$TEST_LOG"

if [ $TESTS_FAILED -gt 0 ]; then
    echo "  Failed Tests:" | tee -a "$TEST_LOG"
    for test in "${FAILED_TESTS[@]}"; do
        echo "    - $test" | tee -a "$TEST_LOG"
    done
    echo "" | tee -a "$TEST_LOG"
    echo "❌ INTEGRATION TESTS FAILED" | tee -a "$TEST_LOG"
    echo "Log file: $TEST_LOG"
    exit 1
else
    echo "" | tee -a "$TEST_LOG"
    echo "✅ ALL INTEGRATION TESTS PASSED" | tee -a "$TEST_LOG"
    echo "Log file: $TEST_LOG"
    exit 0
fi