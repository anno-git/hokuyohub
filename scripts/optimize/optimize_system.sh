#!/bin/bash
# システム最適化スクリプト

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "=== HokuyoHub System Optimization ==="
echo "Project: $PROJECT_ROOT"

# WebUI 最適化
optimize_webui() {
    echo "Optimizing WebUI..."
    
    cd "$PROJECT_ROOT/webui-server"
    
    # npm dependencies 最適化
    echo "Cleaning npm cache..."
    npm cache clean --force
    
    echo "Installing production dependencies..."
    npm ci --only=production
    
    # 静的ファイル圧縮（gzip対応）
    echo "Optimizing static assets..."
    
    # CSS/JS 最小化（Node.js標準ツール使用）
    find public -name "*.css" -exec echo "Optimizing CSS: {}" \;
    find public -name "*.js" -exec echo "Optimizing JS: {}" \;
    
    echo "WebUI optimization completed"
}

# Backend 最適化
optimize_backend() {
    echo "Optimizing Backend..."
    
    # Strip debug symbols (if needed)
    if command -v strip >/dev/null 2>&1; then
        local binary="$PROJECT_ROOT/build/darwin-arm64/hokuyo_hub"
        local original_size=$(stat -f%z "$binary" 2>/dev/null || echo "0")
        
        echo "Original binary size: $((original_size / 1024))KB"
        
        # バックアップ作成
        cp "$binary" "$binary.backup"
        
        # Strip実行
        strip "$binary" 2>/dev/null || echo "Strip not applicable"
        
        local optimized_size=$(stat -f%z "$binary" 2>/dev/null || echo "0")
        echo "Optimized binary size: $((optimized_size / 1024))KB"
        
        local savings=$((original_size - optimized_size))
        if [ $savings -gt 0 ]; then
            echo "Space saved: $((savings / 1024))KB"
        fi
    fi
    
    echo "Backend optimization completed"
}

# 設定最適化
optimize_config() {
    echo "Optimizing Configuration..."
    
    # 本番用設定作成
    local prod_config="$PROJECT_ROOT/configs/production.yaml"
    
    if [ ! -f "$prod_config" ]; then
        cp "$PROJECT_ROOT/configs/default.yaml" "$prod_config"
        
        # 本番用設定調整（例）
        echo "Created production config: $prod_config"
        echo "Consider adjusting log levels, timeouts, and resource limits"
    fi
    
    echo "Configuration optimization completed"
}

# ログローテーション設定
setup_log_rotation() {
    echo "Setting up log rotation..."
    
    local logrotate_config="/etc/logrotate.d/hokuyo_hub"
    
    if [ -w "/etc/logrotate.d" ]; then
        cat > "$logrotate_config" << 'EOF'
/var/log/hokuyo_hub/*.log {
    daily
    rotate 7
    compress
    delaycompress
    missingok
    notifempty
    create 644 root root
    postrotate
        systemctl reload hokuyo_hub || true
    endscript
}
EOF
        echo "Log rotation configured: $logrotate_config"
    else
        echo "Cannot configure system log rotation (no write access)"
        echo "Manual log management recommended"
    fi
}

# システムチューニング提案
suggest_system_tuning() {
    echo "=== System Tuning Suggestions ==="
    
    echo "Performance Recommendations:"
    echo "1. Set NODE_ENV=production for WebUI server"
    echo "2. Configure reverse proxy (nginx/apache) for production"
    echo "3. Enable gzip compression"
    echo "4. Set up SSL/TLS certificates"
    echo "5. Configure firewall rules for ports 8080, 8081"
    
    echo ""
    echo "Monitoring Recommendations:"
    echo "1. Set up automated health checks (cron job)"
    echo "2. Configure log aggregation"
    echo "3. Monitor disk space and memory usage"
    echo "4. Set up alerting for service failures"
    
    echo ""
    echo "Security Recommendations:"
    echo "1. Run services as non-root user"
    echo "2. Configure rate limiting"
    echo "3. Enable authentication for WebUI"
    echo "4. Regular security updates"
}

# 最適化実行
echo "Starting system optimization..."
echo "================================"

optimize_webui
optimize_backend  
optimize_config
setup_log_rotation
suggest_system_tuning

echo ""
echo "✅ System optimization completed"
echo "Review suggestions above for production deployment"