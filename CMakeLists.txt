cmake_minimum_required(VERSION 3.18)
project(hokuyo_hub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# -------- install 先（デフォルト: ${CMAKE_SOURCE_DIR}/dist。ただしユーザ指定で上書き可）--------
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/dist" CACHE PATH "Install path prefix" FORCE)
endif()

option(ENABLE_NNG "Enable NNG publisher" ON)

# 依存：Drogon, yaml-cpp, (optional) nng
find_package(Drogon REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)

if(ENABLE_NNG)
  find_path(NNG_INCLUDE_DIR nng/nng.h)
  find_library(NNG_LIBRARY nng)
  if(NOT NNG_INCLUDE_DIR OR NOT NNG_LIBRARY)
    message(FATAL_ERROR "NNG enabled but not found. Set -DENABLE_NNG=OFF or install nng.")
  endif()
endif()

# =========================
# urg_library (Makefile系) を ExternalProject でビルド
# =========================
include(ExternalProject)

set(URG_SRC_DIR ${CMAKE_SOURCE_DIR}/external/urg_library/release/urg_library-1.2.7)
set(URG_INSTALL_DIR ${CMAKE_SOURCE_DIR}/third_party/urg_library)
set(URG_INCLUDE_DIR ${URG_INSTALL_DIR}/include)
set(URG_LIB_DIR     ${URG_INSTALL_DIR}/lib)

# 事前に作成（IMPORTEDの存在チェックでコケないように）
file(MAKE_DIRECTORY ${URG_INSTALL_DIR})
file(MAKE_DIRECTORY ${URG_INCLUDE_DIR})
file(MAKE_DIRECTORY ${URG_LIB_DIR})

# 1) Cライブラリだけビルド（installは呼ばない）
ExternalProject_Add(urg_library_proj
  SOURCE_DIR        ${URG_SRC_DIR}
  CONFIGURE_COMMAND ""
  BUILD_COMMAND     ${CMAKE_MAKE_PROGRAM} -C ${URG_SRC_DIR}/src
  INSTALL_COMMAND   ""   # installは呼ばない
  BUILD_IN_SOURCE   1
  LOG_BUILD         1
)

# 2) ビルド後、必要ファイルだけコピー
ExternalProject_Add_Step(urg_library_proj copy_headers
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${URG_SRC_DIR}/include/c ${URG_INCLUDE_DIR}
  DEPENDEES build
  ALWAYS 1
)

ExternalProject_Add_Step(urg_library_proj copy_library
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${URG_SRC_DIR}/src/liburg_c.a ${URG_LIB_DIR}/liburg_c.a
  DEPENDEES build
  ALWAYS 1
)

# 3) IMPORTEDライブラリとして宣言（このパスを指す）
add_library(urg_c STATIC IMPORTED GLOBAL)
set_target_properties(urg_c PROPERTIES
  IMPORTED_LOCATION             ${URG_LIB_DIR}/liburg_c.a
  INTERFACE_INCLUDE_DIRECTORIES ${URG_INCLUDE_DIR}
)
add_dependencies(urg_c urg_library_proj)

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

target_include_directories(sensor_core PUBLIC
  src
  ${URG_INCLUDE_DIR}
)
target_link_libraries(sensor_core PUBLIC urg_c)

# =========================
# メイン実行ファイル
# =========================
add_executable(hokuyo_hub
  src/main.cpp
  src/config/config.cpp
  src/core/sensor_manager.cpp
  src/detect/dbscan.cpp
  src/io/nng_bus.cpp
  src/io/rest_handlers.cpp
  src/io/ws_handlers.cpp
)

target_include_directories(hokuyo_hub PRIVATE src)
target_link_libraries(hokuyo_hub PRIVATE
  Drogon::Drogon
  yaml-cpp::yaml-cpp
  sensor_core
)

if(ENABLE_NNG)
  target_compile_definitions(hokuyo_hub PRIVATE USE_NNG)
  target_include_directories(hokuyo_hub PRIVATE ${NNG_INCLUDE_DIR})
  target_link_libraries(hokuyo_hub PRIVATE ${NNG_LIBRARY})
endif()

# =========================
# install（配布レイアウト）
# =========================
# 実行バイナリはインストール先のトップへ
install(TARGETS hokuyo_hub
  RUNTIME DESTINATION .
)

# 設定ファイル（configs/ 配下を丸ごと）
install(DIRECTORY ${CMAKE_SOURCE_DIR}/configs/
        DESTINATION config
        FILES_MATCHING PATTERN "*")

# WebUI（webui/ 配下を丸ごと）
install(DIRECTORY ${CMAKE_SOURCE_DIR}/webui/
        DESTINATION webui
        FILES_MATCHING PATTERN "*")

# 実行時の動的ライブラリ解決（必要な場合のみ有効化してOK）
# set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
# list(APPEND CMAKE_INSTALL_RPATH "/opt/homebrew/lib")