# hokuyo-hub-starter (MVP)

## 要件（今回の叩き台）
- ETH の Hokuyo を複数扱う（USBは後日）
- 60Hzを想定したパイプライン（現状はスタブ）
- 配信: NNG + MessagePack（core）
- UI: WebSocket + JSON + Canvas
- 設定: YAML（センサーごとに angle/range、world polygon 共通）

## 依存のインストール（Ubuntu/Debian/Raspberry Pi OS）
```bash
# Drogon
sudo apt-get update
sudo apt-get install -y cmake g++ pkg-config git libjsoncpp-dev uuid-dev \
  zlib1g-dev libssl-dev libbrotli-dev
# Drogon 本体（Pi では apt にある場合はそれでOK）
sudo apt-get install -y libdrogon-dev || {
  git clone https://github.com/drogonframework/drogon && cd drogon && \
  git submodule update --init && mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && \
  make -j$(nproc) && sudo make install && cd ../../
}

# yaml-cpp
sudo apt-get install -y libyaml-cpp-dev

# nng（有効にする場合）
sudo apt-get install -y cmake ninja-build
git clone https://github.com/nanomsg/nng.git && cd nng && mkdir build && cd build \
  && cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DNNG_TESTS=OFF -DNNG_TOOLS=OFF .. \
  && ninja && sudo ninja install && sudo ldconfig

ビルド

mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_NNG=ON ..
make -j$(nproc)

実行

./hokuyo_hub --config ./default.yaml --listen 0.0.0.0:8080 --pub tcp://0.0.0.0:5555
# ブラウザ： http://<PiのIP>:8080/

systemd（後日）

/etc/systemd/system/hokuyo-hub.service を作成して自動起動。

[Service]
ExecStart=/usr/local/bin/hokuyo_hub --config /etc/hokuyo-hub/config.yaml --listen 0.0.0.0:8080 --pub tcp://0.0.0.0:5555

```
