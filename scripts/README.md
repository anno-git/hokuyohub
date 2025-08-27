# Scripts Directory Structure

This directory contains all build, setup, testing, and utility scripts organized into logical subdirectories for better maintainability and clarity.

## Directory Structure

```
scripts/
├── build/                    # Core build scripts
├── setup/                   # Environment setup scripts
├── testing/                 # Testing and validation scripts
├── utils/                   # Utility scripts and wrappers
└── legacy/                  # Deprecated scripts (to be replaced)
```

## Subdirectories

### `build/` - Core Build Scripts
Contains the main build scripts that compile and package the application:

- **[`cross_build.sh`](build/cross_build.sh)** - Primary cross-compilation script supporting multiple architectures (Linux x86_64, ARM64, Raspberry Pi). Well-structured 300+ line script with comprehensive error handling and logging.
- **[`build_with_presets.sh`](build/build_with_presets.sh)** - Build script using predefined configuration presets for different target environments.

### `setup/` - Environment Setup Scripts  
Contains scripts for setting up build environments and dependencies:

- **[`setup_cross_compile.sh`](setup/setup_cross_compile.sh)** - Sets up cross-compilation toolchains and environment variables. Well-structured script with proper validation and error handling.

### `testing/` - Testing and Validation Scripts
Contains all testing, validation, and quality assurance scripts:

- **[`test_rest_api.sh`](testing/test_rest_api.sh)** - REST API testing script for validating web service endpoints and functionality.
- **[`baseline_verification.sh`](testing/baseline_verification.sh)** - Baseline verification script to ensure core functionality meets requirements.
- **[`regression_check.sh`](testing/regression_check.sh)** - Regression testing script to validate that changes don't break existing functionality.

### `utils/` - Utility Scripts and Wrappers
Contains utility scripts and simple wrappers that support the build process:

- **[`extract_docker_artifacts.sh`](utils/extract_docker_artifacts.sh)** - Utility script for extracting build artifacts from Docker containers.
- **[`preset_build.sh`](utils/preset_build.sh)** - Simple wrapper script that calls the main build scripts with predefined arguments.

### `legacy/` - Deprecated Scripts
Contains scripts that are deprecated and scheduled for replacement:

- **[`run_macos.command`](legacy/run_macos.command)** - Simple 3-line macOS command script. To be replaced with more robust solution.

## Usage Guidelines

- Scripts should be executed from the project root directory
- All scripts maintain their original functionality and interfaces
- Scripts in `legacy/` should be avoided for new development
- Cross-references between scripts use relative paths to maintain portability

## Script Relationships

- [`utils/preset_build.sh`](utils/preset_build.sh) calls [`build/build_with_presets.sh`](build/build_with_presets.sh) with predefined parameters
- [`build/cross_build.sh`](build/cross_build.sh) references [`setup/setup_cross_compile.sh`](setup/setup_cross_compile.sh) for environment setup
- All other scripts are independent and can be run standalone

## Common Usage Examples

### Cross-Compilation Build
```bash
# Set up cross-compilation environment (one-time setup)
./scripts/setup/setup_cross_compile.sh

# Build for Raspberry Pi 5
./scripts/build/cross_build.sh --preset rpi-release --install

# Or use the convenience wrapper
./scripts/utils/preset_build.sh release
```

### Native macOS Build
```bash
# Build debug configuration
./scripts/build/build_with_presets.sh debug --install

# Build release configuration
./scripts/build/build_with_presets.sh release --install --test
```

### Testing and Validation
```bash
# Run API tests
./scripts/testing/test_rest_api.sh

# Verify baseline functionality
./scripts/testing/baseline_verification.sh

# Run regression tests
./scripts/testing/regression_check.sh
```

## Maintenance Notes

- All scripts preserve their original permissions and functionality
- Path references have been updated to reflect the new directory structure  
- No script content has been modified except for path references where necessary
- This reorganization maintains backward compatibility while improving organization

## Phase 2 Reorganization

This directory structure was implemented as part of **Phase 2** of the build system reorganization to:

- ✅ Create logical groupings of related scripts
- ✅ Improve maintainability and discoverability
- ✅ Separate concerns (build, setup, testing, utilities)
- ✅ Preserve all existing functionality
- ✅ Maintain executable permissions and script interfaces
- ✅ Update cross-references between scripts

The reorganization ensures that all existing workflows continue to function while providing a cleaner, more maintainable structure for future development.