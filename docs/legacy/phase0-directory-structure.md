# Phase 0 Directory Structure Documentation

## Project Root Structure

```
proj/
├── .gitignore                        # Git ignore rules
├── .gitmodules                       # Git submodule configuration
├── CMakeLists.txt                    # Main CMake build configuration
├── CMakePresets.json                 # CMake build presets
├── README.md                         # Project documentation
├── build/                            # Build artifacts organized by platform
│   ├── darwin-arm64/                 # macOS ARM64 build artifacts
│   │   ├── Debug/                    # Debug build artifacts
│   │   ├── Release/                  # Release build artifacts
│   │   └── baseline/                 # Phase 0 baseline artifacts (git-ignored)
│   │       ├── baseline/             # Reference baseline artifacts
│   │       └── current/              # Current build artifacts for comparison
│   └── linux-arm64/                  # Linux ARM64 (Raspberry Pi) build artifacts
│       ├── Debug/                    # Debug build artifacts
│       └── Release/                  # Release build artifacts
├── dist/                             # Installation artifacts organized by platform
│   ├── darwin-arm64/                 # macOS ARM64 installation directory
│   │   ├── Debug/                    # Debug installation
│   │   └── Release/                  # Release installation
│   └── linux-arm64/                  # Linux ARM64 (Raspberry Pi) installation directory
│       ├── Debug/                    # Debug installation
│       └── Release/                  # Release installation
├── third_party/                      # External library build artifacts by platform
│   ├── darwin-arm64/                 # macOS ARM64 third-party libraries
│   │   └── urg_library/              # Built urg_library artifacts
│   └── linux-arm64/                  # Linux ARM64 third-party libraries
│       └── urg_library/              # Built urg_library artifacts
├── .cache/                           # Build cache organized by platform
│   ├── ccache/                       # Compiler cache
│   │   ├── darwin-arm64/             # macOS ARM64 compiler cache
│   │   └── linux-arm64/              # Linux ARM64 compiler cache
│   └── fetchcontent/                 # CMake FetchContent cache
├── cmake/                            # CMake configuration files
├── configs/                          # Application configuration files
├── docs/                             # Documentation
├── external/                         # Third-party dependencies (submodules)
├── scripts/                          # Build and utility scripts
├── src/                              # Application source code
└── webui/                            # Web interface assets
```

## Detailed Directory Analysis

### Build System Core

#### `/` (Project Root)
- **CMakeLists.txt**: Main build configuration
  - C++20 standard, CMake 3.18+ required
  - Defines `hokuyo_hub` executable and `sensor_core` library
  - Handles external dependencies via ExternalProject
  - Install configuration for distribution layout

- **CMakePresets.json**: Build preset definitions
  - `mac-base`, `mac-debug`, `mac-release`: macOS native builds
  - `rpi-base`: Raspberry Pi cross-compilation (placeholder)
  - Uses Homebrew prefix `/opt/homebrew` for macOS

#### `/cmake/` - Build System Configuration
```
cmake/
├── toolchains/
│   ├── common.cmake                  # Shared toolchain utilities
│   └── linux-arm64.cmake            # Linux ARM64 cross-compilation toolchain
└── test_sources/                     # CMake test configurations
```

**Key Files**:
- **common.cmake**: Cross-compilation utility functions
  - Tool validation and discovery
  - Sysroot configuration
  - ExternalProject environment setup
- **linux-arm64.cmake**: Complete ARM64 toolchain
  - Raspberry Pi 5 optimizations (Cortex-A76)
  - Comprehensive tool detection
  - Sysroot and pkg-config setup

### Source Code Organization

#### `/src/` - Application Source
```
src/
├── main.cpp                          # Application entry point
├── config/
│   └── config.cpp                    # Configuration management
├── core/
│   ├── sensor_manager.cpp            # Sensor lifecycle management
│   ├── filter_manager.cpp            # Data filtering pipeline
│   └── mask.cpp                      # ROI masking functionality
├── detect/
│   ├── dbscan.cpp                    # DBSCAN clustering algorithm
│   ├── prefilter.cpp                 # Pre-processing filters
│   └── postfilter.cpp                # Post-processing filters
├── io/
│   ├── nng_bus.cpp                   # NNG message bus
│   ├── osc_publisher.cpp             # OSC output publisher
│   ├── publisher_manager.cpp         # Publisher coordination
│   ├── rest_handlers.cpp             # REST API endpoints
│   └── ws_handlers.cpp               # WebSocket handlers
└── sensors/
    ├── ISensor.h                     # Sensor interface
    ├── SensorFactory.h/.cpp          # Sensor factory pattern
    └── hokuyo/
        ├── HokuyoSensorUrg.h         # Hokuyo URG sensor implementation
        └── HokuyoSensorUrg.cpp
```

### External Dependencies

#### `/external/` - Third-Party Libraries
```
external/
├── the-previous-web-framework/                           # Web framework (git submodule)
├── urg_library/                      # Hokuyo sensor library
│   ├── current/                      # Symlink to active version
│   └── release/
│       └── urg_library-1.2.7/       # Specific version
└── yaml-cpp/                         # YAML parser (git submodule)
```

**Dependency Management**:
- **CrowCpp**: Web framework (header-only)
- **yaml-cpp**: YAML parsing, found via `find_package(yaml-cpp CONFIG REQUIRED)`
- **urg_library**: Built via ExternalProject from source in `external/urg_library/release/urg_library-1.2.7`

#### `/third_party/` - Built Dependencies
```
third_party/
├── darwin-arm64/                     # macOS ARM64 third-party libraries
│   └── urg_library/                  # Built urg_library artifacts
│       ├── include/                  # Headers copied from source
│       └── lib/
│           └── liburg_c.a            # Static library
└── linux-arm64/                     # Linux ARM64 third-party libraries
    └── urg_library/                  # Built urg_library artifacts
        ├── include/                  # Headers copied from source
        └── lib/
            └── liburg_c.a            # Static library
```

### Configuration and Assets

#### `/configs/` - Application Configuration
```
configs/
└── default.yaml                      # Default application configuration
```

#### `/webui/` - Web Interface
```
webui/
├── index.html                        # Main web interface
├── styles.css                        # Styling
├── app.js.backup                     # Backup file
└── js/                               # JavaScript modules
    ├── api.js                        # API communication
    ├── app.js                        # Main application logic
    ├── canvas.js                     # Canvas rendering
    ├── configs.js                    # Configuration management
    ├── dbscan.js                     # DBSCAN parameter UI
    ├── filters.js                    # Filter configuration UI
    ├── roi.js                        # ROI management
    ├── sensors.js                    # Sensor management UI
    ├── sinks.js                      # Output sink configuration
    ├── store.js                      # State management
    ├── types.js                      # Type definitions
    ├── utils.js                      # Utility functions
    └── ws.js                         # WebSocket communication
```

### Scripts and Tools

#### `/scripts/` - Build and Utility Scripts
```
scripts/
├── baseline_verification.sh          # Phase 0: Baseline capture and comparison
├── regression_check.sh               # Phase 0: Automated regression detection
├── run_macos.command                 # macOS execution helper
└── test_rest_api.sh                  # API smoke tests
```

**Script Capabilities**:
- **baseline_verification.sh**: Build verification, artifact capture, regression detection
- **regression_check.sh**: Comprehensive regression testing (build + API)
- **test_rest_api.sh**: Complete REST API validation suite

### Documentation

#### `/docs/` - Project Documentation
```
docs/
├── implementation-plans.md           # Main implementation documentation
├── phase0-change-management.md       # Phase 0 change management workflow
├── phase0-directory-structure.md     # This document
└── plans/                            # Feature implementation plans
    ├── ROIの数値編集を可能にする.md
    ├── サーバアプリ再起動機能（未保存確認・サービス前提）.md
    ├── topic購読の実装を検討.md
    └── 通信リザルト表示の精査.md
```

### Build Artifacts

#### Build Directories
- **`/build/darwin-arm64/`**: macOS ARM64 build artifacts
  - `Debug/`: Debug build configuration
  - `Release/`: Release build configuration
  - `baseline/`: Phase 0 baseline artifacts (git-ignored)
- **`/build/linux-arm64/`**: Linux ARM64 (Raspberry Pi) build artifacts
  - `Debug/`: Debug build configuration
  - `Release/`: Release build configuration

#### Installation Directories
- **`/dist/darwin-arm64/`**: macOS ARM64 installation artifacts
  - `Debug/`: Debug installation
  - `Release/`: Release installation
- **`/dist/linux-arm64/`**: Linux ARM64 (Raspberry Pi) installation artifacts
  - `Debug/`: Debug installation
  - `Release/`: Release installation

#### Phase 0 Baseline System
- **`/build/darwin-arm64/baseline/`**: Phase 0 baseline artifacts (git-ignored)
  - `baseline/`: Reference baseline for regression detection
  - `current/`: Current build artifacts for comparison

## Dependency Flow

### Build Dependencies
```
CMakeLists.txt
├── External: CrowCpp (header-only)
├── External: yaml-cpp (Homebrew/system)
├── ExternalProject: urg_library
│   ├── Source: external/urg_library/release/urg_library-1.2.7
│   ├── Build: Makefile-based build
│   └── Install: third_party/{platform}/urg_library/
└── Targets:
    ├── sensor_core (static library)
    │   └── Depends: urg_c (IMPORTED from urg_library)
    └── hokuyo_hub (executable)
        ├── Depends: sensor_core
        ├── Depends: Crow::Crow
        └── Depends: yaml-cpp::yaml-cpp
```

### Runtime Dependencies
```
hokuyo_hub (executable)
├── Dynamic Libraries:
│   ├── CrowCpp (header-only)
│   ├── yaml-cpp
│   ├── System libraries (macOS/Linux)
│   └── Optional: NNG (if enabled)
├── Configuration:
│   └── configs/default.yaml
└── Web Assets:
    └── webui/ (entire directory)
```

## Cross-Compilation Structure

### Current State
- **Native Build**: Fully functional macOS build system
- **Cross-Compilation**: Toolchain exists (`cmake/toolchains/linux-arm64.cmake`) but not integrated with presets
- **Target Platform**: Linux ARM64 (Raspberry Pi 5)

### Phase 1 Integration Plan
The existing `linux-arm64.cmake` toolchain will be integrated with CMakePresets.json to enable:
- Proper cross-compilation workflow
- Automated cross-build testing
- Target-specific optimization and configuration

## File Permissions and Executables

### Executable Scripts
- `scripts/baseline_verification.sh` (755)
- `scripts/regression_check.sh` (755)
- `scripts/run_macos.command` (755)
- `scripts/test_rest_api.sh` (755)

### Build Outputs
- `dist/{platform}/{config}/hokuyo_hub` (executable)
- `third_party/{platform}/urg_library/lib/liburg_c.a` (static library)

## Git Configuration

### Submodules
- `external/the-previous-web-framework`: Web framework
- `external/yaml-cpp`: YAML parser

### Ignored Paths
- Build directories: `build/`, `dist/`
- Temporary files: `third_party/`, `.cache/`
- System files: `.DS_Store`

This directory structure provides a solid foundation for the cross-compilation build system implementation, with clear separation of concerns and comprehensive tooling for safe development practices.