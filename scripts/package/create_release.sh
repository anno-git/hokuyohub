#!/bin/bash
# HokuyoHub リリースパッケージ作成スクリプト

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
VERSION="${1:-$(date +%Y%m%d_%H%M%S)}"
RELEASE_DIR="$PROJECT_ROOT/releases/hokuyo_hub_$VERSION"

echo "=== HokuyoHub Release Package Creator ==="
echo "Version: $VERSION"
echo "Release Directory: $RELEASE_DIR"

# リリースディレクトリ作成
mkdir -p "$RELEASE_DIR"

# 必要なファイルをコピー
echo "Copying application files..."

# バイナリ
mkdir -p "$RELEASE_DIR/bin"
cp "$PROJECT_ROOT/build/darwin-arm64/hokuyo_hub" "$RELEASE_DIR/bin/" || {
    echo "Error: Backend binary not found. Please build first."
    exit 1
}

# WebUI server
mkdir -p "$RELEASE_DIR/webui-server"
cp -r "$PROJECT_ROOT/webui-server/"* "$RELEASE_DIR/webui-server/"
# node_modules は除外
rm -rf "$RELEASE_DIR/webui-server/node_modules"

# 設定ファイル
mkdir -p "$RELEASE_DIR/configs"
cp -r "$PROJECT_ROOT/configs/"* "$RELEASE_DIR/configs/"

# スクリプト
mkdir -p "$RELEASE_DIR/scripts"
cp "$PROJECT_ROOT/scripts/start_system.sh" "$RELEASE_DIR/scripts/"
cp "$PROJECT_ROOT/scripts/stop_system.sh" "$RELEASE_DIR/scripts/"
cp "$PROJECT_ROOT/scripts/restart_backend.sh" "$RELEASE_DIR/scripts/"
cp "$PROJECT_ROOT/scripts/install/install.sh" "$RELEASE_DIR/scripts/"

# ドキュメント
mkdir -p "$RELEASE_DIR/docs"
cp "$PROJECT_ROOT/README.md" "$RELEASE_DIR/docs/" 2>/dev/null || echo "README.md not found"
cp "$PROJECT_ROOT/QUICK_START.md" "$RELEASE_DIR/docs/" 2>/dev/null || echo "QUICK_START.md not found"

# ライセンス・設定例
[ -f "$PROJECT_ROOT/LICENSE" ] && cp "$PROJECT_ROOT/LICENSE" "$RELEASE_DIR/"

# インストール用README作成
cat > "$RELEASE_DIR/README_INSTALL.md" << 'EOF'
# HokuyoHub Installation Guide

## Prerequisites
- Node.js 18+ 
- C++ Runtime libraries
- Port 8080, 8081 available

## Installation
1. Run the installer:
   ```bash
   cd scripts
   ./install.sh
   ```

2. Start the system:
   ```bash
   ./scripts/start_system.sh
   ```

3. Access WebUI: http://localhost:8080

## Directory Structure
- `bin/` - Backend executable
- `webui-server/` - WebUI server
- `configs/` - Configuration files  
- `scripts/` - Operation scripts
- `docs/` - Documentation

## Operation Scripts
- `scripts/start_system.sh` - Start both WebUI and backend
- `scripts/stop_system.sh` - Stop entire system
- `scripts/restart_backend.sh` - Restart only backend

For detailed usage, see docs/QUICK_START.md
EOF

# パッケージ情報ファイル作成
cat > "$RELEASE_DIR/package_info.json" << EOF
{
  "name": "HokuyoHub",
  "version": "$VERSION",
  "created": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "architecture": "darwin-arm64",
  "components": {
    "backend": "C++ API Server (port 8081)",
    "webui": "Node.js Express Server (port 8080)",
    "proxy": "HTTP/WebSocket proxy integration"
  },
  "requirements": {
    "node": ">=18.0.0",
    "ports": [8080, 8081]
  }
}
EOF

# 権限設定
chmod +x "$RELEASE_DIR/bin/hokuyo_hub"
chmod +x "$RELEASE_DIR/scripts/"*.sh

# アーカイブ作成
ARCHIVE_PATH="$PROJECT_ROOT/releases/hokuyo_hub_$VERSION.tar.gz"
echo "Creating archive: $ARCHIVE_PATH"
cd "$PROJECT_ROOT/releases"
tar -czf "hokuyo_hub_$VERSION.tar.gz" "hokuyo_hub_$VERSION/"

echo "=== Release Package Created ==="
echo "Directory: $RELEASE_DIR"
echo "Archive: $ARCHIVE_PATH"
echo "Size: $(du -h "$ARCHIVE_PATH" | cut -f1)"
ls -la "$ARCHIVE_PATH"