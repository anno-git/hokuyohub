#!/bin/bash
# 設定復元スクリプト

BACKUP_PATH="$1"
CONFIG_DIR="/opt/hokuyo_hub/configs"

if [ -z "$BACKUP_PATH" ]; then
    echo "Usage: $0 <backup_directory>"
    exit 1
fi

if [ ! -d "$BACKUP_PATH" ]; then
    echo "Error: Backup directory not found: $BACKUP_PATH"
    exit 1
fi

echo "Restoring configuration from: $BACKUP_PATH"

# 現在の設定をバックアップ
if [ -d "$CONFIG_DIR" ]; then
    CURRENT_BACKUP="/tmp/hokuyo_hub_config_current_$(date +%Y%m%d_%H%M%S)"
    cp -r "$CONFIG_DIR" "$CURRENT_BACKUP"
    echo "✓ Current config backed up to: $CURRENT_BACKUP"
fi

# 復元実行
if [ -d "$BACKUP_PATH/configs" ]; then
    rm -rf "$CONFIG_DIR"
    cp -r "$BACKUP_PATH/configs" "$CONFIG_DIR"
    echo "✓ Configuration restored"
else
    echo "Error: configs directory not found in backup"
    exit 1
fi

echo "Configuration restoration completed"
echo "Please restart the system to apply changes"