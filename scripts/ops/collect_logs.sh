#!/bin/bash
# ログ収集スクリプト

LOG_DIR="/tmp/hokuyo_hub_logs_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$LOG_DIR"

echo "Collecting HokuyoHub logs to: $LOG_DIR"

# システムログ
if command -v journalctl &> /dev/null; then
    echo "Collecting systemd logs..."
    journalctl -u hokuyo_hub --no-pager > "$LOG_DIR/systemd.log" 2>/dev/null || echo "No systemd logs found"
fi

# プロセス情報
echo "Collecting process information..."
ps aux | grep -E "(hokuyo_hub|node.*webui)" > "$LOG_DIR/processes.txt"

# ネットワーク情報
echo "Collecting network information..."
netstat -ln | grep -E ":808[01]" > "$LOG_DIR/ports.txt"

# 設定ファイル
if [ -f "/opt/hokuyo_hub/configs/local.yaml" ]; then
    cp "/opt/hokuyo_hub/configs/local.yaml" "$LOG_DIR/config.yaml"
fi

# システム情報
echo "Collecting system information..."
{
    echo "=== System Information ==="
    uname -a
    echo ""
    echo "=== Disk Space ==="
    df -h
    echo ""
    echo "=== Memory Usage ==="
    free -h
    echo ""
    echo "=== Load Average ==="
    uptime
} > "$LOG_DIR/system_info.txt"

# アーカイブ作成
ARCHIVE_PATH="/tmp/hokuyo_hub_logs_$(date +%Y%m%d_%H%M%S).tar.gz"
tar -czf "$ARCHIVE_PATH" -C "/tmp" "$(basename "$LOG_DIR")"

echo "Logs collected: $ARCHIVE_PATH"
echo "Archive size: $(du -h "$ARCHIVE_PATH" | cut -f1)"

# クリーンアップ
rm -rf "$LOG_DIR"