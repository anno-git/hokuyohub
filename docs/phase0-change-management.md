# Phase 0 Change Management Workflow

## Overview

This document establishes the change management workflow for Phase 0 of the HokuyoHub cross-compilation build system implementation. The workflow ensures that all changes are safe, reversible, and properly validated against established baselines.

## Safety Net Architecture

### 1. Baseline System

**Baseline Capture**: [`scripts/baseline_verification.sh`](../scripts/baseline_verification.sh)
- Captures current build state as reference point
- Stores binary artifacts, dependencies, and build metrics
- Location: `build/darwin-arm64/baseline/baseline/` (excluded from git)

**Baseline Components**:
- `binary_size.txt`: Executable size in bytes
- `dependencies.txt`: Dynamic library dependencies (otool -L output)
- `architecture.txt`: Binary architecture information
- `checksum.txt`: MD5 checksum of binary
- `installed_files.txt`: Complete list of installed files
- `build_timestamp.txt`: Build completion timestamp
- `system_info.txt`: Build environment information
- `CMakeCache.txt`: CMake configuration snapshot

### 2. Regression Detection

**Regression Check**: [`scripts/regression_check.sh`](../scripts/regression_check.sh)
- Automated regression detection against baseline
- Multiple check modes: quick, full, build-only, api-only
- Integration with existing API test suite

**Detection Capabilities**:
- Build failure detection
- Binary size changes
- Dependency changes
- API functionality regressions
- Installation artifact changes

### 3. API Validation

**API Test Suite**: [`scripts/test_rest_api.sh`](../scripts/test_rest_api.sh)
- Comprehensive REST API smoke tests
- Validates all endpoints and functionality
- Integrated with regression detection system

## Change Management Process

### Pre-Change Checklist

1. **Establish Baseline** (if not already done):
   ```bash
   ./scripts/baseline_verification.sh --capture-baseline --clean
   ```

2. **Verify Current State**:
   ```bash
   ./scripts/regression_check.sh --quick
   ```

3. **Document Planned Changes**:
   - Update relevant documentation
   - Note expected impacts on build system
   - Identify rollback strategy

### During Implementation

1. **Incremental Changes**:
   - Make small, focused changes
   - Test each change independently
   - Commit frequently with descriptive messages

2. **Continuous Validation**:
   ```bash
   # Quick check after each change
   ./scripts/regression_check.sh --build-only
   
   # Full validation before commits
   ./scripts/regression_check.sh --full
   ```

### Post-Change Validation

1. **Full Regression Check**:
   ```bash
   ./scripts/regression_check.sh --full --verbose
   ```

2. **Baseline Comparison**:
   ```bash
   ./scripts/baseline_verification.sh --compare-baseline --verbose
   ```

3. **Update Baseline** (if changes are intentional):
   ```bash
   ./scripts/baseline_verification.sh --capture-baseline
   ```

## Rollback Procedures

### Git-Based Rollback

1. **Identify Last Known Good State**:
   ```bash
   git log --oneline -10
   ```

2. **Rollback to Previous Commit**:
   ```bash
   git reset --hard <commit-hash>
   ```

3. **Verify Rollback**:
   ```bash
   ./scripts/regression_check.sh --quick
   ```

### Build System Rollback

1. **Clean Build Environment**:
   ```bash
   ./scripts/baseline_verification.sh --clean
   ```

2. **Rebuild from Clean State**:
   ```bash
   ./scripts/baseline_verification.sh --capture-baseline --clean
   ```

### Configuration Rollback

1. **Backup Current Configuration**:
   ```bash
   cp CMakeLists.txt CMakeLists.txt.backup
   cp CMakePresets.json CMakePresets.json.backup
   ```

2. **Restore from Backup**:
   ```bash
   cp CMakeLists.txt.backup CMakeLists.txt
   cp CMakePresets.json.backup CMakePresets.json
   ```

## Phase Transition Guidelines

### Phase 0 → Phase 1 Preparation

1. **Final Baseline Capture**:
   ```bash
   ./scripts/baseline_verification.sh --capture-baseline --clean --verbose
   ```

2. **Full System Validation**:
   ```bash
   ./scripts/regression_check.sh --full --verbose
   ```

3. **Documentation Update**:
   - Update [`docs/implementation-plans.md`](implementation-plans.md)
   - Document any discovered issues or limitations
   - Note performance baselines

4. **Backup Critical Files**:
   ```bash
   cp CMakePresets.json CMakePresets.json.phase0
   cp cmake/toolchains/linux-arm64.cmake cmake/toolchains/linux-arm64.cmake.phase0
   ```

### Phase 1 Implementation Safety

1. **Preserve Phase 0 State**:
   - Keep Phase 0 baseline in `build/darwin-arm64/baseline/phase0/`
   - Maintain Phase 0 configuration backups
   - Document Phase 0 → Phase 1 changes

2. **Incremental Validation**:
   - Test each CMakePresets.json change
   - Validate toolchain integration
   - Ensure backward compatibility

## Emergency Procedures

### Build System Corruption

1. **Immediate Actions**:
   ```bash
   # Stop any running processes
   pkill -f hokuyo_hub || true
   
   # Clean all build artifacts
   rm -rf build dist third_party
   
   # Reset to last known good state
   git status
   git stash  # if needed
   git reset --hard HEAD~1  # or specific commit
   ```

2. **Recovery Validation**:
   ```bash
   ./scripts/baseline_verification.sh --capture-baseline --clean --verbose
   ./scripts/regression_check.sh --full --verbose
   ```

### Dependency Issues

1. **Homebrew Dependencies**:
   ```bash
   brew update
   brew upgrade drogon yaml-cpp
   ```

2. **External Dependencies**:
   ```bash
   # Clean external build artifacts
   rm -rf third_party
   
   # Rebuild from clean state
   ./scripts/baseline_verification.sh --clean --capture-baseline
   ```

## Monitoring and Alerts

### Key Metrics to Monitor

- **Build Time**: Track build duration changes
- **Binary Size**: Monitor executable size growth
- **Dependency Count**: Watch for dependency bloat
- **Test Coverage**: Ensure API test coverage remains complete

### Warning Thresholds

- Binary size increase > 10%: Investigate
- Build time increase > 50%: Investigate
- New dependencies: Review necessity
- API test failures: Immediate attention required

## Tools and Scripts Summary

| Script | Purpose | Usage |
|--------|---------|-------|
| [`baseline_verification.sh`](../scripts/baseline_verification.sh) | Capture and compare build baselines | `--capture-baseline`, `--compare-baseline` |
| [`regression_check.sh`](../scripts/regression_check.sh) | Automated regression detection | `--quick`, `--full`, `--build-only` |
| [`test_rest_api.sh`](../scripts/test_rest_api.sh) | API functionality validation | `[base_url] [token]` |
| [`run_macos.command`](../scripts/run_macos.command) | macOS execution helper | Direct execution |

## Next Phase Preparation

Phase 1 will focus on:
1. Integrating existing `linux-arm64.cmake` toolchain with CMakePresets.json
2. Establishing proper cross-compilation workflows
3. Validating cross-compilation builds against macOS baseline

The safety nets established in Phase 0 will continue to protect against regressions during Phase 1 implementation.