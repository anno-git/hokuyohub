# Dual Distribution Deployment Guide

## Overview

HokuyoHub supports two deployment methods that can both be made public even with a private repository:

1. **🐳 Docker Containers** - Complete runtime environment (208MB)
2. **📦 Standalone Binaries** - Lightweight ARM64 binaries (3.2MB)

## Making GitHub Assets Public

### Step 1: Make Container Packages Public

**For Docker Images:**
1. Go to your GitHub repository
2. Navigate to **Settings → Packages**
3. Find `hokuyohub` package
4. Click **Package settings**
5. Change **Visibility** → **Public**

This allows anyone to pull Docker images:
```bash
docker pull ghcr.io/[username]/hokuyohub:latest
```

### Step 2: Configure Public Releases

**GitHub Releases are automatically public when:**
- Repository releases are enabled
- Release assets are created by GitHub Actions
- No additional authentication needed for downloads

## Deployment Options

### 🐳 Option 1: Docker Container (Recommended)

**Advantages:**
- ✅ Zero dependency management
- ✅ Consistent environment across devices
- ✅ Simple one-command deployment
- ✅ Automatic updates via `docker pull`

**Raspberry Pi Deployment:**
```bash
# Pull and run latest version
docker pull ghcr.io/[username]/hokuyohub:latest
docker run -d -p 8080:8080 --name hokuyohub ghcr.io/[username]/hokuyohub:latest

# View logs
docker logs hokuyohub

# Update to newer version
docker pull ghcr.io/[username]/hokuyohub:latest
docker stop hokuyohub && docker rm hokuyohub
docker run -d -p 8080:8080 --name hokuyohub ghcr.io/[username]/hokuyohub:latest
```

### 📦 Option 2: Standalone Binary

**Advantages:**
- ✅ Minimal footprint (3.2MB download)
- ✅ No Docker required
- ✅ Direct system integration
- ✅ Lower resource usage

**Raspberry Pi Deployment:**
```bash
# Download latest release
wget https://github.com/[username]/hokuyohub/releases/latest/download/hokuyohub-arm64-latest.tar.gz

# Extract and install
tar -xzf hokuyohub-arm64-latest.tar.gz
cd hokuyohub-arm64

# Quick installation (recommended)
./install.sh

# Manual installation
chmod +x hokuyo_hub
./hokuyo_hub --config config/default.yaml
```

## Automated Build Pipeline

### GitHub Actions Triggers

| Trigger | Docker Image | Binary Release | Visibility |
|---------|-------------|----------------|------------|
| Push to `main` | `ghcr.io/[username]/hokuyohub:main` | Artifacts only | Public* |
| Push to `develop` | `ghcr.io/[username]/hokuyohub:develop` | Artifacts only | Public* |
| Tag `v1.0.0` | `ghcr.io/[username]/hokuyohub:v1.0.0` | GitHub Release | Public |
| Pull Request | Build only | Build only | Private |

*_Requires manual package visibility change_

### Release Process

**For Public Release:**
1. Create and push version tag:
   ```bash
   git tag v1.0.0
   git push origin v1.0.0
   ```

2. GitHub Actions automatically:
   - Builds ARM64 Docker image
   - Creates deployment package with installation script
   - Publishes to GitHub Container Registry
   - Creates GitHub Release with binary assets
   - Includes usage instructions in release notes

**For Development Builds:**
```bash
git push origin main  # Triggers build + Docker publish
```

## User Experience Comparison

### Docker Container User Flow
```bash
# One-time setup
docker pull ghcr.io/[username]/hokuyohub:latest
docker run -d -p 8080:8080 --name hokuyohub ghcr.io/[username]/hokuyohub:latest

# Updates
docker pull ghcr.io/[username]/hokuyohub:latest
docker restart hokuyohub
```

### Standalone Binary User Flow
```bash
# One-time setup
wget https://github.com/[username]/hokuyohub/releases/latest/download/hokuyohub-arm64-latest.tar.gz
tar -xzf hokuyohub-arm64-latest.tar.gz
cd hokuyohub-arm64 && ./install.sh

# Updates
wget https://github.com/[username]/hokuyohub/releases/latest/download/hokuyohub-arm64-latest.tar.gz
# Extract and replace binary
```

## Security Considerations

### Private Repository + Public Assets

**What Remains Private:**
- ✅ Source code
- ✅ Repository access
- ✅ Issues and pull requests
- ✅ Development discussions

**What Becomes Public:**
- 📦 Docker container images
- 📦 Binary releases
- 📦 Release notes and usage instructions

**Benefits:**
- Source code intellectual property protected
- Easy distribution to end users
- No authentication required for deployment
- Professional deployment experience

## Monitoring and Analytics

### Docker Hub Metrics
- Pull count and frequency
- Geographic distribution
- Version adoption rates

### GitHub Release Analytics
- Download statistics per release
- Popular deployment methods
- User feedback via issues

## Troubleshooting

### Common Issues

**Docker Pull Fails:**
```bash
# Ensure package is public
# Check: GitHub → Packages → hokuyohub → Visibility
docker pull ghcr.io/[username]/hokuyohub:latest
```

**Binary Missing Dependencies:**
```bash
# Install system libraries
sudo apt update
sudo apt install libdrogon-dev libhdf5-dev libboost-all-dev
```

**Permission Denied:**
```bash
# Make binary executable
chmod +x hokuyo_hub
```

## Next Steps

1. **Test the deployment pipeline** by creating a test tag
2. **Make container packages public** in GitHub settings
3. **Verify download links** work from different networks
4. **Monitor usage metrics** to optimize deployment options
5. **Gather user feedback** to improve installation experience

This dual approach provides maximum flexibility while maintaining source code privacy.