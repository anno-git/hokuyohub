# HokuyoHub Documentation

This directory contains comprehensive documentation for the HokuyoHub project, organized by topic and purpose.

## 📁 Directory Structure

### [build/](build/)
**Build System Documentation**
- [`BUILD_GUIDE.md`](build/BUILD_GUIDE.md) - Complete build instructions for all platforms and deployment targets
- [`DEPLOYMENT_GUIDE.md`](build/DEPLOYMENT_GUIDE.md) - **Dual distribution setup (Docker + binaries)**
- [`GITHUB_ACTIONS.md`](build/GITHUB_ACTIONS.md) - CI/CD automation and release pipeline
- [`BUILD_STATUS.md`](build/BUILD_STATUS.md) - Current build system status and metrics
- [`DOCKER_BUILD_DEBUG_REPORT.md`](build/DOCKER_BUILD_DEBUG_REPORT.md) - Docker build system debugging and troubleshooting

### [development/](development/)
**Development and Planning**
- [`plans/`](development/plans/) - Feature roadmap and implementation plans
  - ROI editing functionality
  - Topic subscription implementation
  - Server restart capabilities
  - Communication result display improvements

### [api/](api/)
**API Documentation** *(Reserved for future API documentation)*

### [legacy/](legacy/)
**Historical Documentation**
- Contains archived documentation from previous build system implementations
- Includes phase-based implementation reports and legacy build procedures
- Reference material for understanding system evolution

## 🚀 Quick Start

### For Developers
1. **Building**: Start with [`build/BUILD_GUIDE.md`](build/BUILD_GUIDE.md)
2. **Scripts**: See [`../scripts/README.md`](../scripts/README.md) for build script usage
3. **Docker Issues**: Check [`build/DOCKER_BUILD_DEBUG_REPORT.md`](build/DOCKER_BUILD_DEBUG_REPORT.md)

### For Users
1. **Main README**: See [`../README.md`](../README.md) for application overview
2. **Configuration**: Build guide contains configuration examples
3. **Troubleshooting**: Build guide includes common issues and solutions

## 📋 Documentation Standards

### File Organization
- **build/** - All build system related documentation
- **development/** - Planning, roadmap, and development guides  
- **api/** - API reference and examples
- **legacy/** - Historical documentation archive

### Cross-References
- All documentation includes clear cross-references to related files
- Build documentation links to relevant scripts in `../scripts/`
- Main project README links to organized documentation sections

## 🔗 Key Documentation Links

| Topic | Primary Document | Supporting Files |
|-------|------------------|------------------|
| **Building** | [`build/BUILD_GUIDE.md`](build/BUILD_GUIDE.md) | [`../scripts/README.md`](../scripts/README.md) |
| **Deployment** | [`build/DEPLOYMENT_GUIDE.md`](build/DEPLOYMENT_GUIDE.md) | Public Docker + binary distribution |
| **CI/CD Pipeline** | [`build/GITHUB_ACTIONS.md`](build/GITHUB_ACTIONS.md) | Automated builds and releases |
| **Docker Build Issues** | [`build/DOCKER_BUILD_DEBUG_REPORT.md`](build/DOCKER_BUILD_DEBUG_REPORT.md) | Docker build scripts |
| **Development Plans** | [`development/plans/`](development/plans/) | Implementation roadmaps |
| **Project Overview** | [`../README.md`](../README.md) | Main project documentation |

## 📝 Documentation Updates

This documentation structure was established to:

1. **Improve Organization**: Clear separation of build, development, and reference materials
2. **Reduce Redundancy**: Consolidated build information in comprehensive guides
3. **Enhance Discoverability**: Logical grouping with clear navigation
4. **Support Maintenance**: Well-organized structure for ongoing updates

### Recent Changes
- Reorganized documentation into logical categories
- Created comprehensive build guide consolidating multiple sources
- Documented Docker build debugging process and solutions
- Updated main README with modern build system information
- Archived legacy documentation while maintaining accessibility

---

*For specific documentation needs, start with the appropriate category above or consult the main project README.*