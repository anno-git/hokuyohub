#!/bin/bash
# HokuyoHub 自動インストーラ

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="/opt/hokuyo_hub"
SERVICE_NAME="hokuyo_hub"

echo "=== HokuyoHub Installer ==="

# 権限チェック
if [ "$EUID" -eq 0 ]; then
    echo "Running as root - system-wide installation"
    SYSTEM_INSTALL=true
else
    echo "Running as user - local installation"
    INSTALL_DIR="$HOME/.local/share/hokuyo_hub"
    SYSTEM_INSTALL=false
fi

# Node.js チェック
if ! command -v node &> /dev/null; then
    echo "Error: Node.js not found. Please install Node.js 18+"
    echo "Visit: https://nodejs.org/"
    exit 1
fi

NODE_VERSION=$(node --version | sed 's/v//')
REQUIRED_VERSION="18.0.0"

if ! printf '%s\n%s\n' "$REQUIRED_VERSION" "$NODE_VERSION" | sort -V -C; then
    echo "Error: Node.js $REQUIRED_VERSION+ required, found $NODE_VERSION"
    exit 1
fi

echo "✓ Node.js $NODE_VERSION found"

# ポート可用性チェック
check_port() {
    local port=$1
    if netstat -ln 2>/dev/null | grep -q ":$port "; then
        echo "Warning: Port $port appears to be in use"
        return 1
    fi
    return 0
}

check_port 8080 || echo "⚠️  Port 8080 may be in use"
check_port 8081 || echo "⚠️  Port 8081 may be in use"

# インストールディレクトリ作成
echo "Installing to: $INSTALL_DIR"
mkdir -p "$INSTALL_DIR"

# ファイルコピー
if [ -d "$SCRIPT_DIR/../bin" ]; then
    cp -r "$SCRIPT_DIR/../"* "$INSTALL_DIR/"
else
    echo "Error: Installation files not found in $SCRIPT_DIR"
    exit 1
fi

# WebUI dependencies インストール
echo "Installing WebUI dependencies..."
cd "$INSTALL_DIR/webui-server"
npm install --production

# 実行権限設定
chmod +x "$INSTALL_DIR/bin/hokuyo_hub"
chmod +x "$INSTALL_DIR/scripts/"*.sh

# 設定ファイル初期化
if [ ! -f "$INSTALL_DIR/configs/local.yaml" ]; then
    cp "$INSTALL_DIR/configs/default.yaml" "$INSTALL_DIR/configs/local.yaml"
    echo "✓ Created local configuration: $INSTALL_DIR/configs/local.yaml"
fi

# システムサービス作成（root実行時のみ）
if [ "$SYSTEM_INSTALL" = true ]; then
    create_systemd_service
fi

# 完了メッセージ
echo ""
echo "=== Installation Complete ==="
echo "Installation Directory: $INSTALL_DIR"
echo ""
echo "To start HokuyoHub:"
echo "  $INSTALL_DIR/scripts/start_system.sh"
echo ""
echo "WebUI will be available at: http://localhost:8080"
echo "Backend API will be at: http://localhost:8081"
echo ""

if [ "$SYSTEM_INSTALL" = true ]; then
    echo "System service created. You can also use:"
    echo "  sudo systemctl start $SERVICE_NAME"
    echo "  sudo systemctl enable $SERVICE_NAME  # Auto-start on boot"
fi

# systemdサービス作成関数
create_systemd_service() {
    cat > "/etc/systemd/system/$SERVICE_NAME.service" << EOF
[Unit]
Description=HokuyoHub LiDAR Processing System
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=$INSTALL_DIR
ExecStart=$INSTALL_DIR/scripts/start_system.sh
ExecStop=$INSTALL_DIR/scripts/stop_system.sh
Restart=always
RestartSec=10

Environment=NODE_ENV=production
Environment=PATH=/usr/local/bin:/usr/bin:/bin

[Install]
WantedBy=multi-user.target
EOF

    systemctl daemon-reload
    echo "✓ Created systemd service: $SERVICE_NAME"
}