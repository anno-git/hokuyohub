const express = require('express');
const { createProxyMiddleware } = require('http-proxy-middleware');
const cors = require('cors');
const path = require('path');
const { spawn } = require('child_process');
const fs = require('fs');
const net = require('net');

const app = express();
const PORT = 8080;
const BACKEND_URL = 'http://127.0.0.1:8081';

// CORS設定
app.use(cors());

// JSON parsing middleware（プロキシ対象外のルートのみ）
app.use('/api/v1/backend', express.json());

// 静的ファイル配信
app.use(express.static(path.join(__dirname, 'public')));

// バックエンドプロセス管理クラス
class BackendManager {
    constructor() {
        this.backendProcess = null;
        this.configPath = process.env.BACKEND_CONFIG || './configs/default.yaml';
        this.listenAddress = process.env.BACKEND_LISTEN || '0.0.0.0:8081';

        // プラットフォームに応じたバイナリパスを自動選択
        this.backendPath = process.env.BACKEND_PATH || this._detectBackendPath();
    }

    _detectBackendPath() {
        const candidates = [
            `../dist/${process.platform === 'linux' ? 'linux' : 'darwin'}-${process.arch === 'arm64' ? 'arm64' : 'x64'}/hokuyo_hub`,
            '../dist/darwin-arm64/hokuyo_hub',
            '../dist/linux-arm64/hokuyo_hub',
            '../build/darwin-arm64/hokuyo_hub',
        ];
        for (const p of candidates) {
            const abs = path.resolve(__dirname, p);
            if (fs.existsSync(abs)) return p;
        }
        return candidates[0];
    }

    async start() {
        if (this.backendProcess) {
            await this.stop();
        }

        const absPath = path.resolve(__dirname, this.backendPath);
        if (!fs.existsSync(absPath)) {
            throw new Error(`Backend binary not found: ${absPath}`);
        }

        const args = [
            '--listen', this.listenAddress,
            '--config', this.configPath
        ];

        this.backendProcess = spawn(absPath, args, {
            cwd: path.resolve(__dirname, '..'),
            stdio: ['ignore', 'pipe', 'pipe']
        });

        // バックエンドの出力をフォワード
        this.backendProcess.stdout.on('data', (data) => {
            process.stdout.write(`[backend] ${data}`);
        });
        this.backendProcess.stderr.on('data', (data) => {
            process.stderr.write(`[backend] ${data}`);
        });
        this.backendProcess.on('exit', (code, signal) => {
            console.log(`[backend] Process exited (code=${code}, signal=${signal})`);
            this.backendProcess = null;
        });

        console.log(`Backend started with PID: ${this.backendProcess.pid} (${this.backendPath})`);
        return this.backendProcess.pid;
    }

    async stop() {
        if (this.backendProcess) {
            this.backendProcess.kill('SIGTERM');
            this.backendProcess = null;
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

                socket.connect(8081, '127.0.0.1', () => {
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

app.listen(PORT, async () => {
    console.log(`WebUI Server running on http://localhost:${PORT}`);
    console.log(`Backend API proxy target: ${BACKEND_URL}`);

    // バックエンド自動起動
    try {
        const status = await backendManager.getStatus();
        if (status.status === 'online') {
            console.log('Backend already running, skipping auto-start');
        } else {
            console.log('Starting backend automatically...');
            await backendManager.start();
        }
    } catch (error) {
        console.error('Failed to auto-start backend:', error.message);
    }

    // Start health checker
    healthChecker.start();
});