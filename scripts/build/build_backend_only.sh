#!/bin/bash

echo "=== バックエンドのみビルド ==="

PROJECT_ROOT="$(dirname "$(dirname "$(dirname "$(realpath "$0")")"))""
cd "$PROJECT_ROOT"

# バックエンドビルド
if ! cmake -B build/darwin-arm64 -DCMAKE_BUILD_TYPE=Release; then
    echo "❌ CMake configuration failed"
    exit 1
fi

if ! cmake --build build/darwin-arm64 --parallel; then
    echo "❌ Backend build failed"
    exit 1
fi

# distディレクトリ作成とバイナリ配置
mkdir -p dist/darwin-arm64/bin

if [ -f "build/darwin-arm64/hokuyo_hub" ]; then
    cp build/darwin-arm64/hokuyo_hub dist/darwin-arm64/bin/
    echo "✅ バックエンドビルド完了"
    echo "バイナリ: dist/darwin-arm64/bin/hokuyo_hub"
else
    echo "❌ Backend binary not found"
    exit 1
fi