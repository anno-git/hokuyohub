cmake_minimum_required(VERSION 3.18)
project(hokuyo_hub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find threading support (required for CrowCpp on Unix systems)
find_package(Threads REQUIRED)

# -------- install 先（デフォルト: ${CMAKE_SOURCE_DIR}/dist。ただしユーザ指定で上書き可）--------
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/dist" CACHE PATH "Install path prefix" FORCE)
endif()

# =========================
# Phase 3: Unified Dependency Management
# =========================
include(cmake/Dependencies.cmake)

# Optional features (non-dependency related)
option(USE_OSC "Enable OSC publisher support" ON)

if(USE_OSC)
  add_definitions(-DUSE_OSC)
  message(STATUS "OSC support enabled")
endif()

# Resolve all dependencies using unified system
resolve_all_dependencies()

# =========================
# センサー層ライブラリ（urg_c に依存）
# =========================
add_library(sensor_core
  src/sensors/ISensor.h
  src/sensors/SensorFactory.h
  src/sensors/SensorFactory.cpp
  src/sensors/hokuyo/HokuyoSensorUrg.h
  src/sensors/hokuyo/HokuyoSensorUrg.cpp
)

target_include_directories(sensor_core PUBLIC src)
target_link_libraries(sensor_core PUBLIC urg_c)

# =========================
# メイン実行ファイル
# =========================
add_executable(hokuyo_hub
  src/main.cpp
  src/config/config.cpp
  src/core/sensor_manager.cpp
  src/core/filter_manager.cpp
  src/detect/dbscan.cpp
  src/detect/prefilter.cpp
  src/detect/postfilter.cpp
  src/io/nng_bus.cpp
  src/io/osc_publisher.cpp
  src/io/publisher_manager.cpp
  src/io/rest_handlers.cpp
  src/io/ws_handlers.cpp
  src/core/mask.cpp
)

target_include_directories(hokuyo_hub PRIVATE src)
target_link_libraries(hokuyo_hub PRIVATE sensor_core)

# Link threading support (required for CrowCpp)
target_link_libraries(hokuyo_hub PRIVATE Threads::Threads)

# Link all dependencies using unified system
link_hokuyo_dependencies(hokuyo_hub)

# =========================
# install（配布レイアウト）
# =========================
# 実行バイナリはインストール先のトップへ
install(TARGETS hokuyo_hub
  RUNTIME DESTINATION .
)

# 設定ファイル（configs/ 配下を丸ごと）
install(DIRECTORY ${CMAKE_SOURCE_DIR}/configs/
        DESTINATION configs
        FILES_MATCHING PATTERN "*")

# WebUI（webui/ 配下を丸ごと）
install(DIRECTORY ${CMAKE_SOURCE_DIR}/webui/
        DESTINATION webui
        FILES_MATCHING PATTERN "*")

# 実行時の動的ライブラリ解決（必要な場合のみ有効化してOK）
# set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
# list(APPEND CMAKE_INSTALL_RPATH "/opt/homebrew/lib")