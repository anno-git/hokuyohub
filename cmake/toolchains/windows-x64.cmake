# =============================================================================
# CMake Toolchain File for Windows x64 (MSVC Native Build)
# =============================================================================
# Native Windows ビルド用のツールチェイン設定。vcpkg と組み合わせて使う。
#
# 使い方:
#   - CMakePresets の "win-release" 経由が推奨
#   - 直接呼ぶ場合は vcpkg toolchain を CMAKE_TOOLCHAIN_FILE で先に渡し、
#     その上でこのファイルを include する形になる（preset がやってくれる）

include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

# =============================================================================
# Target System Configuration
# =============================================================================

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Windows API 最低バージョン: Windows 10 (0x0A00)
add_compile_definitions(_WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)

# MSVC 固有の調整
if(MSVC)
    # UTF-8 ソースを正しく解釈する
    add_compile_options(/utf-8)

    # 多くの C ランタイム警告を抑制（_CRT_SECURE_NO_WARNINGS など）
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _SCL_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
    )

    # std::min/max を Windows.h の min/max マクロから守る（NOMINMAX で対応済みだが念のため）
endif()

message(STATUS "Configuring for Windows x64 native build (MSVC)")
