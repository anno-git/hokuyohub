@echo off
REM HokuyoHub Windows ランチャ
REM Node.js webui-server を起動し、内部で hokuyo_hub.exe を spawn します。
REM ブラウザで http://localhost:8080 を開くと WebUI が見られます。

setlocal
cd /d "%~dp0"

set "BACKEND_PATH=%~dp0hokuyo_hub.exe"
set "BACKEND_CONFIG=%~dp0configs\default.yaml"
set "BACKEND_LISTEN=0.0.0.0:8081"

REM 同梱の Node.js Portable を PATH に追加
set "PATH=%~dp0node-portable;%PATH%"

echo HokuyoHub 起動中... http://localhost:8080 をブラウザで開いてください
cd webui-server
"%~dp0node-portable\node.exe" server.js
