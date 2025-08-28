# HokuyoHub Docker Usage Guide

This guide provides complete instructions for running HokuyoHub on Raspberry Pi and other devices using Docker.

## Quick Start (Recommended Method)

**For Raspberry Pi 4/5 (ARM64) and Intel/AMD systems:**

```bash
# Pull and run the latest image
docker pull ghcr.io/anno-git/hokuyohub:latest
docker run -d \
  --name hokuyohub \
  --restart unless-stopped \
  -p 8080:8080 \
  -p 8081:8081 \
  --privileged \
  -v /dev:/dev \
  ghcr.io/anno-git/hokuyohub:latest
```

**Access the application:**
- Web Interface: `http://your-device-ip:8080`
- REST API: `http://your-device-ip:8080/api/`
- WebSocket: `ws://your-device-ip:8081`

## Advanced Setup with Custom Configuration

```bash
# Create configuration directory
mkdir -p ~/hokuyohub/configs
cd ~/hokuyohub

# Download sample configuration
curl -o configs/default.yaml https://raw.githubusercontent.com/anno-git/hokuyohub/main/configs/default.yaml

# Edit configuration for your sensor setup
nano configs/default.yaml
# Update sensor endpoint: "192.168.104.202:10940" to your sensor's IP

# Run with configuration volume mount
docker run -d \
  --name hokuyohub \
  --restart unless-stopped \
  -p 8080:8080 \
  -p 8081:8081 \
  --privileged \
  -v /dev:/dev \
  -v $(pwd)/configs:/opt/hokuyohub/configs:ro \
  ghcr.io/anno-git/hokuyohub:latest
```

## Using Docker Compose

```bash
# Download compose file
curl -O https://raw.githubusercontent.com/anno-git/hokuyohub/main/docker/docker-compose.yml

# Start services
docker-compose up -d hokuyohub

# View logs
docker-compose logs -f hokuyohub
```

## Available Image Tags

- `ghcr.io/anno-git/hokuyohub:latest` - Latest stable build
- `ghcr.io/anno-git/hokuyohub:develop` - Development builds
- `ghcr.io/anno-git/hokuyohub:v1.2.3` - Specific version releases

## Supported Platforms

- **linux/amd64** - Intel/AMD 64-bit systems
- **linux/arm64** - ARM 64-bit (Raspberry Pi 4+, modern ARM devices)

## Prerequisites

**For Raspberry Pi users:**
```bash
# Install Docker if not already installed
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh
sudo usermod -aG docker $USER
# Log out and back in for group changes to take effect
```

## Key Features

- **Multi-architecture support**: Automatic platform detection
- **Real-time LiDAR visualization**: Web-based interface at port 8080
- **REST API**: Full programmatic access
- **WebSocket streaming**: Real-time data feeds
- **Configuration management**: YAML-based sensor setup
- **DBSCAN clustering**: Advanced point cloud processing
- **Multiple output formats**: NNG, OSC, REST publishing

## Container Details

- **Base Image**: Debian Bookworm Slim
- **Runtime User**: Non-root `hokuyo` user for security
- **Container Size**: ~180-208MB (optimized)
- **Exposed Ports**: 8080 (HTTP), 8081 (WebSocket)
- **Health Checks**: Built-in container monitoring

## Troubleshooting

### Common Issues

**Permission denied accessing /dev devices:**
```bash
# Ensure the container runs with --privileged flag
# Or use specific device access:
docker run -d \
  --name hokuyohub \
  --device=/dev/ttyACM0 \
  --device=/dev/ttyUSB0 \
  -p 8080:8080 \
  -p 8081:8081 \
  ghcr.io/anno-git/hokuyohub:latest
```

**Container won't start:**
```bash
# Check logs for detailed error information
docker logs hokuyohub

# Verify image architecture matches your platform
docker inspect ghcr.io/anno-git/hokuyohub:latest
```

**Network connectivity issues:**
```bash
# Test if ports are accessible
curl http://localhost:8080/api/health

# Check if container is running
docker ps | grep hokuyohub
```

## Container Management

```bash
# Stop the container
docker stop hokuyohub

# Start the container
docker start hokuyohub

# Remove the container
docker rm hokuyohub

# Update to latest image
docker pull ghcr.io/anno-git/hokuyohub:latest
docker stop hokuyohub
docker rm hokuyohub
# Then run the docker run command again
```

## Configuration Files

The container looks for configuration files in `/opt/hokuyohub/configs/`. You can mount your local configuration directory:

```bash
# Mount local configs directory
-v /path/to/your/configs:/opt/hokuyohub/configs:ro
```

Example configuration structure:
```
configs/
├── default.yaml      # Main configuration
├── sensors/          # Sensor-specific configs
└── filters/          # Filter configurations
```

---

The Docker images are automatically built and published whenever changes are pushed to the repository, ensuring users always have access to the latest optimized builds for their platform.