// バックエンド制御機能
class BackendControl {
    constructor() {
        this.statusIndicator = null;
        this.restartButton = null;
        this.init();
    }

    init() {
        this.createControlUI();
        this.startStatusPolling();
        this.bindEvents();
    }

    createControlUI() {
        // ステータス表示エリア作成
        const statusArea = document.createElement('div');
        statusArea.className = 'backend-status-area';
        statusArea.innerHTML = `
            <div class="backend-control">
                <span class="status-label">Backend:</span>
                <span class="status-indicator" id="backend-status">●</span>
                <button class="restart-btn" id="restart-backend">再起動</button>
            </div>
        `;

        // ページ上部に追加（headerの後）
        const header = document.querySelector('header');
        if (header && header.nextSibling) {
            header.parentNode.insertBefore(statusArea, header.nextSibling);
        } else {
            document.body.insertBefore(statusArea, document.body.firstChild);
        }

        this.statusIndicator = document.getElementById('backend-status');
        this.restartButton = document.getElementById('restart-backend');
    }

    bindEvents() {
        this.restartButton.addEventListener('click', async () => {
            await this.restartBackend();
        });
    }

    async restartBackend() {
        try {
            this.restartButton.disabled = true;
            this.restartButton.textContent = '再起動中...';
            this.updateStatus('restarting', '●再起動中');

            const response = await fetch('/api/v1/backend/restart', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                }
            });

            const result = await response.json();

            if (result.status === 'success') {
                this.showMessage('バックエンド再起動が完了しました', 'success');
                this.updateStatus('online', '●オンライン');
            } else {
                this.showMessage(`再起動に失敗しました: ${result.message}`, 'error');
                this.updateStatus('offline', '●オフライン');
            }

        } catch (error) {
            console.error('Restart failed:', error);
            this.showMessage(`再起動エラー: ${error.message}`, 'error');
            this.updateStatus('offline', '●オフライン');
        } finally {
            this.restartButton.disabled = false;
            this.restartButton.textContent = '再起動';
        }
    }

    async checkBackendStatus() {
        try {
            const response = await fetch('/api/v1/backend/status');
            const status = await response.json();

            if (status.status === 'online') {
                this.updateStatus('online', '●オンライン');
            } else {
                this.updateStatus('offline', '●オフライン');
            }

        } catch (error) {
            this.updateStatus('offline', '●オフライン');
        }
    }

    updateStatus(status, text) {
        if (this.statusIndicator) {
            this.statusIndicator.textContent = text;
            this.statusIndicator.className = `status-indicator ${status}`;
        }
    }

    showMessage(message, type) {
        // 通知表示（簡易実装）
        const notification = document.createElement('div');
        notification.className = `notification ${type}`;
        notification.textContent = message;
        document.body.appendChild(notification);

        setTimeout(() => {
            if (document.body.contains(notification)) {
                document.body.removeChild(notification);
            }
        }, 3000);
    }

    startStatusPolling() {
        // 5秒ごとにステータス確認
        this.checkBackendStatus();
        setInterval(() => {
            this.checkBackendStatus();
        }, 5000);
    }
}

// ページ読み込み後に初期化
document.addEventListener('DOMContentLoaded', () => {
    new BackendControl();
});