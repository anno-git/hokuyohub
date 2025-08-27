# HokuyoHub 実装方針集

この文書は、現在のTODO一覧の各項目について実装の方針を示します。具体実装は担当者に委ねます。
作業担当者は実装計画を日本語でdocs/plansディレクトリにmdファイルで作成し、管理者の承認を得てから作業を開始してください。

TODO項目
- レイアウト変更
  - canvas、左バー、右バーの順にすると、狭い画面で使いやすい
- ROIの数値編集を可能にする
- 通信リザルト表示の精査
- サーバアプリ再起動機能
  - 未保存の情報がある場合は確認ダイアログ
  - サービスとして起動していることを前提として良い
- topic購読の実装を検討

---

# Cross-Compilation Build System Implementation

## Phase 0: Safety Nets and Baselines (CURRENT)

### Current Build State Documentation

**Date**: 2025-08-20
**Status**: Phase 0 - Establishing baselines and safety nets

#### Current Build System Overview

The HokuyoHub project currently uses a CMake-based build system with the following characteristics:

**Build Configuration**:
- CMake minimum version: 3.18
- C++ standard: C++20
- Primary target: macOS (Apple Silicon) with Homebrew dependencies
- Cross-compilation support: Partial (Linux ARM64 toolchain exists)

**Current CMakePresets.json Configuration**:
- `mac-base`: macOS build using Homebrew dependencies (`/opt/homebrew`)
- `mac-debug`: Debug build variant
- `mac-release`: Release build variant
- `rpi-base`: Placeholder for Raspberry Pi cross-compilation (references non-existent `cmake/toolchains/rpi.cmake`)

**Dependencies**:
- **the previous web framework**: Web framework (found via `find_package`)
- **yaml-cpp**: YAML parsing (found via `find_package`)
- **urg_library**: Hokuyo sensor library (built via ExternalProject from `external/urg_library/release/urg_library-1.2.7`)
- **Optional**: NNG (message passing), OSC (Open Sound Control)

**Build Targets**:
- `hokuyo_hub`: Main executable
- `sensor_core`: Static library for sensor abstraction layer

**Current Directory Structure**:
```
proj/
├── CMakeLists.txt                    # Main build configuration
├── CMakePresets.json                 # Build presets (mac-*, rpi-base)
├── cmake/
│   └── toolchains/
│       ├── common.cmake              # Shared toolchain utilities
│       └── linux-arm64.cmake        # Linux ARM64 cross-compilation
├── external/                         # Third-party dependencies
│   ├── urg_library/                  # Hokuyo sensor library
│   └── yaml-cpp/                     # YAML parser
├── src/                              # Application source code
├── webui/                            # Web interface assets
├── configs/                          # Configuration files
├── scripts/
│   ├── run_macos.command             # macOS execution script
│   └── test_rest_api.sh              # API testing script
└── build/                            # Build artifacts organized by platform
    ├── darwin-arm64/                 # macOS ARM64 build artifacts
    └── linux-arm64/                  # Linux ARM64 (Raspberry Pi) build artifacts
```

**Current Build Process**:
1. **macOS Native**: `cmake --preset mac-release && cmake --build build`
2. **Installation**: Installs to `./dist/` by default
3. **External Dependencies**: urg_library built via ExternalProject with custom copy steps

**Known Issues**:
- `rpi-base` preset references non-existent `cmake/toolchains/rpi.cmake`
- Cross-compilation toolchain exists (`linux-arm64.cmake`) but not integrated with presets
- No automated build verification or regression testing
- No baseline artifact capture for comparison

#### Phase 0 Objectives

**Safety Nets**:
- [x] Document current build state and dependencies
- [ ] Create baseline build verification script
- [ ] Establish artifact comparison baseline
- [ ] Create regression detection mechanisms

**Baselines**:
- [ ] Capture current macOS build artifacts (binary size, dependencies)
- [ ] Document current build performance metrics
- [ ] Establish known-good configuration snapshots

**Change Management**:
- [ ] Create rollback procedures
- [ ] Establish incremental change workflow
- [ ] Prepare for Phase 1 CMakePresets.json updates

### Next Phase Preview

**Phase 1**: Will update CMakePresets.json to properly integrate the existing `linux-arm64.cmake` toolchain and establish proper cross-compilation workflows.

