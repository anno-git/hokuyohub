const express = require('express');
const { createProxyMiddleware } = require('http-proxy-middleware');
const cors = require('cors');
const path = require('path');
const { spawn } = require('child_process');
const fs = require('fs');
const net = require('net');

const app = express();
const PORT = 8080;
const BACKEND_URL = 'http://localhost:8081';

// CORS設定
app.use(cors());

// JSON parsing middleware
app.use(express.json());

// 静的ファイル配信
app.use(express.static(path.join(__dirname, 'public')));

// バックエンドプロセス管理クラス
class BackendManager {
    constructor() {
        this.backendProcess = null;
        this.backendPath = '../build/darwin-arm64/hokuyo_hub';
        this.configPath = '../configs/default.yaml';
        this.pidFile = '../backend.pid';
    }

    async start() {
        if (this.backendProcess) {
            await this.stop();
        }

        const args = [
            '--listen', '0.0.0.0:8081',
            '--config', this.configPath
        ];

        this.backendProcess = spawn(this.backendPath, args, {
            detached: true,
            stdio: 'pipe'
        });

        // PIDファイル保存
        fs.writeFileSync(this.pidFile, this.backendProcess.pid.toString());
        
        console.log(`Backend started with PID: ${this.backendProcess.pid}`);
        return this.backendProcess.pid;
    }

    async stop() {
        if (this.backendProcess) {
            this.backendProcess.kill('SIGTERM');
            this.backendProcess = null;
        }

        // PIDファイルからも停止
        if (fs.existsSync(this.pidFile)) {
            const pid = fs.readFileSync(this.pidFile, 'utf8').trim();
            try {
                process.kill(parseInt(pid), 'SIGTERM');
                console.log(`Stopped backend process with PID: ${pid}`);
            } catch (error) {
                // プロセスが既に停止している場合は無視
                console.log(`Process ${pid} already stopped`);
            }
            fs.unlinkSync(this.pidFile);
        }
    }

    async restart() {
        await this.stop();
        await new Promise(resolve => setTimeout(resolve, 2000)); // 2秒待機
        return await this.start();
    }

    async getStatus() {
        try {
            // TCP connection check instead of HTTP fetch for better compatibility
            return new Promise((resolve) => {
                const socket = new net.Socket();
                const timeout = setTimeout(() => {
                    socket.destroy();
                    resolve({
                        status: 'offline',
                        error: 'Connection timeout',
                        pid: this.backendProcess ? this.backendProcess.pid : null
                    });
                }, 2000);

                socket.connect(8081, 'localhost', () => {
                    clearTimeout(timeout);
                    socket.destroy();
                    resolve({
                        status: 'online',
                        pid: this.backendProcess ? this.backendProcess.pid : null
                    });
                });

                socket.on('error', (error) => {
                    clearTimeout(timeout);
                    resolve({
                        status: 'offline',
                        error: error.message,
                        pid: this.backendProcess ? this.backendProcess.pid : null
                    });
                });
            });
        } catch (error) {
            return {
                status: 'offline',
                error: error.message,
                pid: null
            };
        }
    }
}

const backendManager = new BackendManager();

// ヘルスチェック機構
class HealthChecker {
    constructor(backendManager) {
        this.backendManager = backendManager;
        this.interval = null;
    }

    start() {
        this.interval = setInterval(async () => {
            const status = await this.backendManager.getStatus();
            if (status.status === 'offline') {
                console.warn('Backend appears to be offline');
                // 必要に応じて自動再起動等の処理
            }
        }, 10000); // 10秒ごと
    }

    stop() {
        if (this.interval) {
            clearInterval(this.interval);
            this.interval = null;
        }
    }
}

const healthChecker = new HealthChecker(backendManager);

// バックエンド再起動API
app.post('/api/v1/backend/restart', async (req, res) => {
    try {
        console.log('Backend restart requested');
        
        const oldPid = backendManager.backendProcess ?
            backendManager.backendProcess.pid : null;
        
        const newPid = await backendManager.restart();
        
        // 起動確認
        let attempts = 0;
        const maxAttempts = 10;
        
        while (attempts < maxAttempts) {
            const status = await backendManager.getStatus();
            if (status.status === 'online') {
                return res.json({
                    status: 'success',
                    message: 'バックエンド再起動完了',
                    old_pid: oldPid,
                    new_pid: newPid
                });
            }
            await new Promise(resolve => setTimeout(resolve, 1000));
            attempts++;
        }
        
        res.status(500).json({
            status: 'error',
            message: 'バックエンド再起動タイムアウト'
        });
        
    } catch (error) {
        console.error('Backend restart failed:', error);
        res.status(500).json({
            status: 'error',
            message: error.message
        });
    }
});

// バックエンド起動API
app.post('/api/v1/backend/start', async (req, res) => {
    try {
        const pid = await backendManager.start();
        res.json({
            status: 'success',
            message: 'バックエンド起動完了',
            pid: pid
        });
    } catch (error) {
        res.status(500).json({
            status: 'error',
            message: error.message
        });
    }
});

// バックエンド停止API
app.post('/api/v1/backend/stop', async (req, res) => {
    try {
        await backendManager.stop();
        res.json({
            status: 'success',
            message: 'バックエンド停止完了'
        });
    } catch (error) {
        res.status(500).json({
            status: 'error',
            message: error.message
        });
    }
});

// バックエンド状態確認API（強化版）
app.get('/api/v1/backend/status', async (req, res) => {
    try {
        const status = await backendManager.getStatus();
        res.json(status);
    } catch (error) {
        res.status(500).json({
            status: 'offline',
            error: error.message
        });
    }
});

// バックエンドAPIプロキシ設定（バックエンド制御API以外をプロキシ）
const apiProxy = createProxyMiddleware({
    target: BACKEND_URL,
    changeOrigin: true,
    pathFilter: (pathname, req) => {
        // バックエンド制御APIは除外
        return !pathname.startsWith('/api/v1/backend/');
    },
    onError: (err, req, res) => {
        console.error('Backend proxy error:', err.message);
        res.status(503).json({
            error: 'Backend service unavailable',
            message: 'バックエンドサーバーに接続できません'
        });
    },
    onProxyReq: (proxyReq, req, res) => {
        console.log(`Proxying ${req.method} ${req.url} to ${BACKEND_URL}`);
    }
});

// API転送ルート（バックエンド制御API以外）
app.use('/api', apiProxy);

// WebSocket プロキシ
const wsProxy = createProxyMiddleware({
    target: BACKEND_URL,
    changeOrigin: true,
    ws: true
});
app.use('/ws', wsProxy);

// SPA対応（すべてのルートをindex.htmlに転送）
app.get('*', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// プロセス終了時のクリーンアップ
process.on('SIGTERM', async () => {
    console.log('SIGTERM received, shutting down gracefully');
    healthChecker.stop();
    await backendManager.stop();
    process.exit(0);
});

process.on('SIGINT', async () => {
    console.log('SIGINT received, shutting down gracefully');
    healthChecker.stop();
    await backendManager.stop();
    process.exit(0);
});

app.listen(PORT, () => {
    console.log(`WebUI Server running on http://localhost:${PORT}`);
    console.log(`Backend API proxy target: ${BACKEND_URL}`);
    
    // Start health checker
    healthChecker.start();
});