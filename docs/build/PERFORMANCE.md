# Build Performance Metrics

## Optimized Docker Matrix Build

- Build Time: Improved via matrix and caching
- Parallel Execution: linux/amd64, linux/arm64, darwin/amd64, darwin/arm64
<!-- WINDOWS BUILDS TEMPORARILY DISABLED: windows/amd64 -->
- Caching: BuildKit layer + URG dep cache + native dependency cache
- Runners: ubuntu-latest, macos-13, macos-latest
<!-- WINDOWS BUILDS TEMPORARILY DISABLED: windows-latest -->

Last updated: $(date -u '+%Y-%m-%d %H:%M:%S UTC')
