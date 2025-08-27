# GitHub Actions CI/CD Pipeline

This document describes the GitHub Actions workflow for automated building, testing, and deployment of HokuyoHub.

## Overview

The CI/CD pipeline automatically:
- **Builds** ARM64 Docker containers for Raspberry Pi 5 deployment
- **Tests** the built application with API validation
- **Scans** for security vulnerabilities
- **Publishes** Docker images to GitHub Container Registry
- **Creates** releases with deployment artifacts

## Workflow Triggers

### Automatic Triggers
- **Push to main/develop**: Full build, test, and publish cycle
- **Pull Requests**: Build and test (no publishing)
- **Version Tags** (`v*`): Full cycle + GitHub release creation

### Manual Triggers
- Workflow can be triggered manually from the GitHub Actions tab

## Jobs Overview

### 1. `build-arm64` - Primary Build Job

**Purpose**: Cross-compile HokuyoHub for ARM64 Linux using Docker

**Key Steps**:
```yaml
- Checkout code
- Set up Docker Buildx for multi-platform builds
- Run: ./scripts/build/docker_cross_build.sh --build-all
- Extract build artifacts to dist/linux-arm64/
- Upload artifacts for other jobs
- Tag and push Docker images to GHCR
```

**Outputs**:
- ARM64 Linux binary: `hokuyo_hub`
- Complete deployment package: `dist/linux-arm64/`
- Docker runtime container: `ghcr.io/[username]/hokuyohub:latest`

### 2. `test-api` - API Testing

**Purpose**: Validate the built application functionality

**Key Steps**:
```yaml
- Download build artifacts
- Start HokuyoHub server
- Run API tests: ./scripts/testing/test_rest_api.sh
```

**Dependencies**: Requires `build-arm64` to complete successfully

### 3. `security-scan` - Vulnerability Assessment

**Purpose**: Security scanning of Docker images using Trivy

**Key Steps**:
```yaml
- Scan Docker image for vulnerabilities
- Upload results to GitHub Security tab
```

**Output**: Security findings in GitHub Security & Insights

### 4. `create-release` - Automated Releases

**Purpose**: Create GitHub releases for version tags

**Triggers**: Only on version tags (e.g., `v1.0.0`, `v1.0.0-beta`)

**Artifacts Created**:
- `hokuyo-hub-[version]-linux-arm64.tar.gz` - Core binary package
- `hokuyo-hub-[version]-deployment.tar.gz` - Complete deployment package

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

#### Using Docker Image
```bash
# Pull the latest image
docker pull ghcr.io/[username]/hokuyohub:latest

# Run container
docker run -p 8080:8080 -p 8081:8081 ghcr.io/[username]/hokuyohub:latest
```

#### Using Release Artifacts
```bash
# Download from GitHub Releases
curl -L -o hokuyo-hub-deployment.tar.gz \
  https://github.com/[username]/hokuyohub/releases/download/v1.0.0/hokuyo-hub-v1.0.0-deployment.tar.gz

# Extract and run
tar -xzf hokuyo-hub-deployment.tar.gz
cd release-package
chmod +x hokuyo_hub
./hokuyo_hub --config configs/default.yaml
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

This CI/CD pipeline provides automated, reliable deployment of HokuyoHub with comprehensive testing and security scanning integrated into the development workflow.