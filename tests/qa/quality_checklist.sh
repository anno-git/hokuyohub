#!/bin/bash
# 品質保証チェックリスト

QA_LOG="/tmp/hokuyo_hub_qa_$(date +%Y%m%d_%H%M%S).log"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "=== HokuyoHub Quality Assurance Checklist ===" | tee "$QA_LOG"
echo "Timestamp: $(date)" | tee -a "$QA_LOG"

QA_PASS=0
QA_FAIL=0

check_item() {
    local item="$1"
    local check_function="$2"
    
    echo -n "✓ $item ... " | tee -a "$QA_LOG"
    
    if $check_function >> "$QA_LOG" 2>&1; then
        echo "PASS" | tee -a "$QA_LOG"
        ((QA_PASS++))
    else
        echo "FAIL" | tee -a "$QA_LOG"
        ((QA_FAIL++))
    fi
}

# コード品質チェック
check_code_quality() {
    echo "--- Code Quality Check ---"
    
    # C++ ソースファイル確認
    local cpp_files=$(find "$PROJECT_ROOT/src" -name "*.cpp" | wc -l)
    if [ "$cpp_files" -eq 0 ]; then
        echo "ERROR: No C++ source files found"
        return 1
    fi
    
    # JavaScript ファイル確認
    local js_files=$(find "$PROJECT_ROOT/webui-server" -name "*.js" | wc -l)
    if [ "$js_files" -eq 0 ]; then
        echo "ERROR: No JavaScript files found"
        return 1
    fi
    
    echo "Code files present: $cpp_files C++, $js_files JS"
    return 0
}

# セキュリティ基本チェック
check_security_basics() {
    echo "--- Security Basics Check ---"
    
    # 実行権限チェック
    if [ -x "$PROJECT_ROOT/build/darwin-arm64/hokuyo_hub" ]; then
        echo "Backend binary has execute permission"
    else
        echo "ERROR: Backend binary not executable"
        return 1
    fi
    
    # 設定ファイル権限
    local config_perms=$(stat -f "%A" "$PROJECT_ROOT/configs/default.yaml" 2>/dev/null || echo "000")
    if [ "$config_perms" -gt 644 ]; then
        echo "WARNING: Config file permissions too permissive: $config_perms"
    fi
    
    return 0
}

# ドキュメント確認
check_documentation() {
    echo "--- Documentation Check ---"
    
    if [ -f "$PROJECT_ROOT/README.md" ] || [ -f "$PROJECT_ROOT/QUICK_START.md" ]; then
        echo "Documentation files present"
        return 0
    else
        echo "WARNING: No documentation files found"
        return 1
    fi
}

# 依存関係確認
check_dependencies() {
    echo "--- Dependencies Check ---"
    
    # Node.js確認
    if command -v node > /dev/null; then
        local node_version=$(node --version)
        echo "Node.js present: $node_version"
    else
        echo "ERROR: Node.js not found"
        return 1
    fi
    
    # npm package.json確認
    if [ -f "$PROJECT_ROOT/webui-server/package.json" ]; then
        echo "WebUI package.json present"
    else
        echo "ERROR: WebUI package.json missing"
        return 1
    fi
    
    return 0
}

# 設定ファイル妥当性
check_config_validity() {
    echo "--- Config Validity Check ---"
    
    if [ -f "$PROJECT_ROOT/configs/default.yaml" ]; then
        # YAML構文チェック（基本的な）
        if python3 -c "import yaml; yaml.safe_load(open('$PROJECT_ROOT/configs/default.yaml'))" 2>/dev/null; then
            echo "YAML config syntax valid"
            return 0
        else
            echo "ERROR: YAML config syntax invalid"
            return 1
        fi
    else
        echo "ERROR: Default config file missing"
        return 1
    fi
}

# バックアップ機能確認
check_backup_functionality() {
    echo "--- Backup Functionality Check ---"
    
    if [ -x "$PROJECT_ROOT/scripts/ops/backup_config.sh" ]; then
        echo "Backup script present and executable"
        return 0
    else
        echo "ERROR: Backup script not found or not executable"
        return 1
    fi
}

# パフォーマンス基準確認
check_performance_standards() {
    echo "--- Performance Standards Check ---"
    
    # バイナリサイズチェック
    local binary_size=$(stat -f%z "$PROJECT_ROOT/build/darwin-arm64/hokuyo_hub" 2>/dev/null || echo "0")
    local size_mb=$((binary_size / 1024 / 1024))
    
    if [ "$size_mb" -gt 100 ]; then
        echo "WARNING: Backend binary large: ${size_mb}MB"
    else
        echo "Backend binary size acceptable: ${size_mb}MB"
    fi
    
    return 0
}

# チェック実行
echo "Starting quality assurance checks..." | tee -a "$QA_LOG"
echo "============================================" | tee -a "$QA_LOG"

check_item "Code Quality" check_code_quality
check_item "Security Basics" check_security_basics
check_item "Documentation" check_documentation
check_item "Dependencies" check_dependencies
check_item "Config Validity" check_config_validity
check_item "Backup Functionality" check_backup_functionality
check_item "Performance Standards" check_performance_standards

# 最終結果
echo "" | tee -a "$QA_LOG"
echo "============================================" | tee -a "$QA_LOG"
echo "QA Results:" | tee -a "$QA_LOG"
echo "  Passed: $QA_PASS" | tee -a "$QA_LOG"
echo "  Failed: $QA_FAIL" | tee -a "$QA_LOG"

if [ $QA_FAIL -eq 0 ]; then
    echo "✅ ALL QA CHECKS PASSED" | tee -a "$QA_LOG"
    exit 0
else
    echo "⚠️  QA ISSUES DETECTED" | tee -a "$QA_LOG"
    exit 1
fi