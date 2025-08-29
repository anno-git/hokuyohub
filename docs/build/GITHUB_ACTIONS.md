# GitHub Actions CI/CD Pipeline

This document describes the GitHub Actions workflow for automated building, testing, and deployment of HokuyoHub.

## Overview

The CI/CD pipeline automatically:
- **Builds** multi-platform binaries (Linux ARM64/AMD64, macOS ARM64/AMD64, Windows AMD64)
- **Tests** the built application with API validation
- **Scans** for security vulnerabilities
- **Publishes** Docker images to GitHub Container Registry
- **Creates** releases with deployment artifacts for all platforms

## Workflow Triggers

### Automatic Triggers
- **Push to main/develop**: Full build, test, and publish cycle
- **Pull Requests**: Build and test (no publishing)
- **Version Tags** (`v*`): Full cycle + GitHub release creation

### Manual Triggers
- Workflow can be triggered manually from the GitHub Actions tab

## Jobs Overview

### 1. `build-matrix` - Multi-Platform Build Matrix

**Purpose**: Build HokuyoHub for all supported platforms using parallel matrix builds

**Supported Platforms**:
- **Linux**: AMD64/ARM64 (Docker builds)
- **macOS**: Intel/Apple Silicon (Native CMake builds)
- **Windows**: AMD64 (Native CMake builds with Visual Studio 2022)

**Key Steps**:
```yaml
- Platform-specific setup (Docker, Homebrew, vcpkg)
- Parallel builds across 5 platforms simultaneously
- Docker builds: Use optimized multi-stage Dockerfiles
- Native builds: CMake with platform-specific toolchains
- Extract and upload artifacts for all platforms
- Tag and push Docker images to GHCR (Linux only)
```

**Outputs**:
- **Linux binaries**: `hokuyo_hub` (AMD64/ARM64)
- **macOS binaries**: `hokuyo_hub` (Intel/Apple Silicon)
- **Windows binaries**: `hokuyo_hub.exe` (AMD64)
- **Docker containers**: `ghcr.io/[username]/hokuyohub:latest`
- **Platform-specific packages**: `dist/{platform}/`

### 2. `test-matrix` - Multi-Platform Testing

**Purpose**: Validate the built application functionality across platforms

**Key Steps**:
```yaml
- Test Docker containers (Linux AMD64/ARM64)
- Container startup validation
- API endpoint testing
- Runtime dependency verification
```

**Dependencies**: Requires `build-matrix` to complete successfully

### 3. `security-scan` - Vulnerability Assessment

**Purpose**: Security scanning of Docker images using Trivy

**Key Steps**:
```yaml
- Scan Docker image for vulnerabilities
- Upload results to GitHub Security tab
```

**Output**: Security findings in GitHub Security & Insights

### 4. `release` - Automated Multi-Platform Releases

**Purpose**: Create GitHub releases for version tags with all platform artifacts

**Triggers**: Only on version tags (e.g., `v1.0.0`, `v1.0.0-beta`)

**Artifacts Created**:
- `hokuyohub-[version]-linux-amd64.tar.gz` - Linux x64 package
- `hokuyohub-[version]-linux-arm64.tar.gz` - Linux ARM64 package (Raspberry Pi 5)
- `hokuyohub-[version]-darwin-amd64.tar.gz` - macOS Intel package
- `hokuyohub-[version]-darwin-arm64.tar.gz` - macOS Apple Silicon package
- `hokuyohub-[version]-windows-amd64.zip` - Windows x64 package

### 5. `update-docs` - Documentation Maintenance

**Purpose**: Automatically update documentation and badges

**Key Steps**:
```yaml
- Update build status badges in README
- Update build dates in documentation
- Commit changes back to repository
```

## Using the Pipeline

### For Development

1. **Create Feature Branch**:
   ```bash
   git checkout -b feature/my-feature
   git push -u origin feature/my-feature
   ```

2. **Open Pull Request**:
   - Pipeline runs build and test jobs
   - No publishing occurs
   - Status checks must pass for merge

3. **Merge to Main**:
   - Full pipeline executes
   - Docker images published to GHCR
   - Documentation updated

### For Releases

1. **Create Version Tag**:
   ```bash
   git tag v1.0.0
   git push origin v1.0.0
   ```

2. **Automated Release**:
   - Complete build and test cycle
   - GitHub release created automatically
   - Deployment packages attached to release

### For Deployment

#### Using Docker Image (Linux)
```bash
# Pull the latest image
docker pull ghcr.io/[username]/hokuyohub:latest

# Run container
docker run -p 8080:8080 -p 8081:8081 ghcr.io/[username]/hokuyohub:latest
```

#### Using Platform-Specific Release Artifacts

**Linux (Ubuntu/Debian)**:
```bash
# Download and extract
curl -L -o hokuyohub-linux-arm64.tar.gz \
  https://github.com/[username]/hokuyohub/releases/download/v1.0.0/hokuyohub-v1.0.0-linux-arm64.tar.gz
tar -xzf hokuyohub-linux-arm64.tar.gz
chmod +x hokuyo_hub
./hokuyo_hub --config configs/default.yaml
```

**macOS**:
```bash
# Download and extract
curl -L -o hokuyohub-darwin-arm64.tar.gz \
  https://github.com/[username]/hokuyohub/releases/download/v1.0.0/hokuyohub-v1.0.0-darwin-arm64.tar.gz
tar -xzf hokuyohub-darwin-arm64.tar.gz
chmod +x hokuyo_hub
./hokuyo_hub --config configs/default.yaml
```

**Windows**:
```cmd
# Download from GitHub Releases page and extract
# Or use PowerShell
Invoke-WebRequest -Uri "https://github.com/[username]/hokuyohub/releases/download/v1.0.0/hokuyohub-v1.0.0-windows-amd64.zip" -OutFile "hokuyohub-windows.zip"
Expand-Archive -Path "hokuyohub-windows.zip" -DestinationPath "hokuyohub"
cd hokuyohub
.\hokuyo_hub.exe --config configs\default.yaml
```

## Configuration

### Required Secrets

The workflow uses built-in GitHub tokens - no additional secrets required.

### Optional Configurations

#### Custom Docker Registry
```yaml
env:
  REGISTRY: docker.io  # Change from ghcr.io
  IMAGE_NAME: my-org/hokuyohub
```

#### Build Parameters
```yaml
- name: Build with custom options
  run: |
    # Modify build script parameters
    ./scripts/build/docker_cross_build.sh --build-app --verbose
```

## Monitoring and Troubleshooting

### Build Status

Check build status:
- **Badge**: ![Build Status](https://github.com/[username]/hokuyohub/actions/workflows/build.yml/badge.svg)
- **Actions Tab**: Navigate to repository → Actions → Build workflows

### Common Issues

#### 1. Docker Build Failures
```yaml
# Check build logs in the build-arm64 job
# Look for specific error messages:
# - "URG library linking errors"
# - "CMake configuration failed" 
# - "Docker buildx issues"
```

**Solutions**:
- Verify Dockerfile.rpi is up to date
- Check that build scripts have execute permissions
- Review Docker build debug report: [`DOCKER_BUILD_DEBUG_REPORT.md`](DOCKER_BUILD_DEBUG_REPORT.md)

#### 2. API Test Failures
```yaml
# Check test-api job logs
# Common issues:
# - Server startup timeout
# - Port conflicts
# - API endpoint changes
```

**Solutions**:
- Update API test script for new endpoints
- Adjust server startup timeout
- Check configuration file compatibility

#### 3. Permission Issues
```yaml
# Ensure scripts are executable
- name: Fix permissions
  run: |
    chmod +x ./scripts/build/docker_cross_build.sh
    chmod +x ./scripts/testing/test_rest_api.sh
```

### Performance Optimization

#### Cache Docker Layers
```yaml
- name: Set up Docker Buildx
  uses: docker/setup-buildx-action@v3
  with:
    buildkitd-flags: --cache-from type=gha --cache-to type=gha,mode=max
```

#### Parallel Jobs
```yaml
jobs:
  build-arm64:
    # ... existing job
  
  build-docs:
    runs-on: ubuntu-latest
    # Can run in parallel with build-arm64
```

## Integration with Development Workflow

### Pre-commit Hooks
```bash
# Ensure local builds work before pushing
./scripts/build/docker_cross_build.sh --build-all
./scripts/testing/test_rest_api.sh http://localhost:8080
```

### Local Testing
```bash
# Test the same commands locally
chmod +x ./scripts/build/docker_cross_build.sh
./scripts/build/docker_cross_build.sh --build-all

# Verify artifacts
ls -la dist/linux-arm64/
file dist/linux-arm64/hokuyo_hub
```

## Security Considerations

### Image Scanning
- Trivy scanner runs automatically on all builds
- Results available in GitHub Security tab
- Failed scans don't block deployment (configurable)

### Package Publishing
- Images published to GitHub Container Registry (GHCR)
- Uses GitHub token authentication
- Public repository = public images

### Artifact Storage
- Build artifacts stored for 30 days
- Available for download from Actions tab
- Contains deployment-ready packages

## Maintenance

### Updating the Workflow

1. **Modify `.github/workflows/build.yml`**
2. **Test changes on feature branch**
3. **Merge to main after validation**

### Monitoring Resource Usage
- Check job execution times in Actions tab
- Monitor storage usage for artifacts
- Review Docker image sizes in GHCR

### Regular Updates
- Update action versions (e.g., `actions/checkout@v4` → `v5`)
- Review and update security scanning tools
- Update base images in Dockerfile as needed

---

This CI/CD pipeline provides automated, reliable multi-platform deployment of HokuyoHub with comprehensive testing and security scanning integrated into the development workflow.

## Windows Build Integration

### Key Windows Features

**Visual Studio 2022 Integration**:
- Native CMake builds using VS2022 toolchain
- vcpkg dependency management for consistent library versions
- Automatic Windows SDK and runtime library detection

**Platform-Specific Optimizations**:
- Windows-specific URG library serial communication
- MSVC legacy runtime compatibility for older Windows versions
- Windows service integration support

**Deployment Artifacts**:
- Self-contained Windows executables with minimal dependencies
- Windows installer packages (planned for future releases)
- Portable distribution packages for easy deployment