# HokuyoHub サーバー再起動機能 - 本番環境セットアップ

## 概要
HokuyoHubの再起動機能を本番環境で有効化するための設定手順です。現在「Production restart commands not available, simulating restart」というログが表示される場合は、以下の手順で実際の再起動機能を有効化できます。

## 🔧 macOS環境での設定

### 1. Homebrew Services使用の場合

```bash
# サービスとして登録
brew services start hokuyohub

# 再起動機能が以下のコマンドを使用するように設定
# brew services restart hokuyohub
```

### 2. launchctl使用の場合

```bash
# plistファイルを作成
sudo tee /Library/LaunchDaemons/io.hokuyohub.plist << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>io.hokuyohub</string>
    <key>ProgramArguments</key>
    <array>
        <string>/usr/local/bin/hokuyohub</string>
        <string>--config</string>
        <string>/usr/local/etc/hokuyohub/config.yaml</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>StandardOutPath</key>
    <string>/usr/local/var/log/hokuyohub.log</string>
    <key>StandardErrorPath</key>
    <string>/usr/local/var/log/hokuyohub.error.log</string>
</dict>
</plist>
EOF

# サービス登録
sudo launchctl load /Library/LaunchDaemons/io.hokuyohub.plist
sudo launchctl kickstart system/io.hokuyohub

# 再起動機能が以下のコマンドを使用
# sudo launchctl kickstart -k system/io.hokuyohub
```

## 🐧 Linux環境での設定

### systemd使用の場合

```bash
# systemdサービスファイルを作成
sudo tee /etc/systemd/system/hokuyohub.service << 'EOF'
[Unit]
Description=Hokuyo Hub LiDAR Processing Service
After=network.target

[Service]
Type=simple
User=hokuyo
Group=hokuyo
ExecStart=/usr/local/bin/hokuyohub --config /etc/hokuyohub/config.yaml
ExecReload=/bin/kill -HUP $MAINPID
Restart=always
RestartSec=3

# 環境変数
Environment=HOME=/home/hokuyo
Environment=PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

# ログ設定
StandardOutput=journal
StandardError=journal
SyslogIdentifier=hokuyohub

[Install]
WantedBy=multi-user.target
EOF

# サービス有効化
sudo systemctl daemon-reload
sudo systemctl enable hokuyohub
sudo systemctl start hokuyohub

# 再起動機能が以下のコマンドを使用
# sudo systemctl restart hokuyohub
```

## 🔒 セキュリティ設定

### sudoers設定（Linux/macOS共通）

```bash
# sudoers設定を編集
sudo visudo

# 以下を追加（hokuyoユーザーがパスワードなしで再起動コマンドを実行可能）
hokuyo ALL=(ALL) NOPASSWD: /bin/systemctl restart hokuyohub
hokuyo ALL=(ALL) NOPASSWD: /bin/launchctl kickstart -k system/io.hokuyohub
hokuyo ALL=(ALL) NOPASSWD: /usr/local/bin/brew services restart hokuyohub
```

### 実行ユーザーの設定

```bash
# hokuyoユーザーでサーバーを実行
sudo useradd -r -s /bin/false hokuyo
sudo mkdir -p /home/hokuyo
sudo chown hokuyo:hokuyo /home/hokuyo
```

## 📋 環境変数の設定

### 1. 本番環境フラグの設定

```bash
# 環境変数を設定
export HOKUYO_PRODUCTION=1
export HOKUYO_SERVICE_NAME=hokuyohub

# またはサービスファイル内で設定
Environment=HOKUYO_PRODUCTION=1
Environment=HOKUYO_SERVICE_NAME=hokuyohub
```

### 2. 設定ファイルでの指定

```yaml
# config.yaml
system:
  production_mode: true
  service_name: "hokuyohub"
  restart_method: "systemctl"  # "systemctl", "launchctl", or "brew"
```

## 🧪 動作確認

### 1. サービス状態確認

```bash
# Linux
sudo systemctl status hokuyohub

# macOS (launchctl)
sudo launchctl list | grep io.hokuyohub

# macOS (brew)
brew services list | grep hokuyohub
```

### 2. 再起動テスト

```bash
# REST APIを直接テスト
curl -X POST \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $(echo -n "test_$(date +%s)" | base64)" \
  http://localhost:8080/api/v1/server/restart

# ログ確認
tail -f /usr/local/var/log/hokuyohub.log  # macOS
tail -f /var/log/syslog | grep hokuyohub  # Linux
```

## ⚠️ トラブルシューティング

### よくある問題と解決方法

1. **Permission Denied**
   ```bash
   # sudoers設定を確認
   sudo visudo -c
   
   # ユーザーのsudo権限確認
   sudo -l -U hokuyo
   ```

2. **Service Not Found**
   ```bash
   # サービスファイルの存在確認
   ls -la /etc/systemd/system/hokuyohub.service
   
   # systemd再読み込み
   sudo systemctl daemon-reload
   ```

3. **Command Not Found**
   ```bash
   # PATHに必要なコマンドが含まれているか確認
   which systemctl
   which launchctl
   which brew
   ```

## 🔄 実装の流れ

現在の[`executeRestartCommand()`](src/io/rest_handlers.cpp:1580-1622)は以下の順序でコマンドを試行します：

### macOS:
1. **launchctl**: `launchctl kickstart -k system/io.hokuyohub` (line 1584)
2. **brew services**: `brew services restart hokuyohub` (line 1590)
3. **フォールバック**: シミュレーション (line 1597)

### Linux:
1. **systemctl**: `systemctl restart hokuyohub` (line 1602)  
2. **フォールバック**: シミュレーション (line 1609)

上記の設定を行うことで、フォールバックではなく実際の再起動が実行されるようになります。

## 📝 次のステップ

1. **サービス登録**: 上記の設定手順を実行環境に応じて実施
2. **権限設定**: sudoers設定で適切な権限を付与
3. **サービス起動**: systemd/launchctl/brewでサービスを開始
4. **テスト実行**: WebUIからの再起動機能をテスト
5. **ログ確認**: 実際の再起動ログを確認

設定完了後は「Production restart commands not available, simulating restart」ではなく、以下のような実際の再起動ログが表示されるようになります：

- `[RestApi] Server restart initiated via launchctl`
- `[RestApi] Server restart initiated via brew services`  
- `[RestApi] Server restart initiated via systemctl`

## 🚨 重要な注意事項

- **権限管理**: sudoers設定は最小権限の原則に従って設定してください
- **サービス名**: 実際のサービス名が`hokuyohub`と異なる場合は適宜修正してください
- **ログ確認**: 再起動後は必ずログを確認してサービスが正常に起動していることを確認してください
- **バックアップ**: 設定変更前は必ず現在の設定をバックアップしてください