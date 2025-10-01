#!/bin/bash
# 設定バックアップスクリプト

BACKUP_DIR="${1:-/tmp/hokuyo_hub_backup_$(date +%Y%m%d_%H%M%S)}"
CONFIG_DIR="/opt/hokuyo_hub/configs"

echo "Creating configuration backup: $BACKUP_DIR"

mkdir -p "$BACKUP_DIR"

# 設定ファイルバックアップ
if [ -d "$CONFIG_DIR" ]; then
    cp -r "$CONFIG_DIR" "$BACKUP_DIR/"
    echo "✓ Configuration files backed up"
else
    echo "⚠️  Configuration directory not found: $CONFIG_DIR"
fi

# PIDファイルも含める（実行状態の復元用）
if [ -f "/opt/hokuyo_hub/backend.pid" ]; then
    cp "/opt/hokuyo_hub/backend.pid" "$BACKUP_DIR/"
fi

# バックアップ情報
{
    echo "Backup created: $(date)"
    echo "Source: $CONFIG_DIR"
    echo "Destination: $BACKUP_DIR"
    echo "Files:"
    find "$BACKUP_DIR" -type f
} > "$BACKUP_DIR/backup_info.txt"

echo "Backup completed: $BACKUP_DIR"