cmake_minimum_required(VERSION 3.18)
project(hokuyo_hub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ENABLE_NNG "Enable NNG publisher" ON)

# 依存：Drogon, yaml-cpp, (optional) nng
find_package(Drogon REQUIRED)
find_package(yaml-cpp REQUIRED)

if(ENABLE_NNG)
  find_path(NNG_INCLUDE_DIR nng/nng.h)
  find_library(NNG_LIBRARY nng)
  if(NOT NNG_INCLUDE_DIR OR NOT NNG_LIBRARY)
    message(FATAL_ERROR "NNG enabled but not found. Set -DENABLE_NNG=OFF or install nng.")
  endif()
endif()

add_executable(hokuyo_hub
  src/main.cpp
  src/config/config.cpp
  src/core/sensor_manager.cpp
  src/core/hokuyo_scip.cpp
  src/detect/dbscan.cpp
  src/io/nng_bus.cpp
  src/io/rest_handlers.cpp
  src/io/ws_handlers.cpp
)

target_include_directories(hokuyo_hub PRIVATE src)

target_link_libraries(hokuyo_hub PRIVATE Drogon::Drogon yaml-cpp)

if(ENABLE_NNG)
  target_compile_definitions(hokuyo_hub PRIVATE USE_NNG)
  target_include_directories(hokuyo_hub PRIVATE ${NNG_INCLUDE_DIR})
  target_link_libraries(hokuyo_hub PRIVATE ${NNG_LIBRARY})
endif()

# webui をランタイムにコピー（簡易）
add_custom_command(TARGET hokuyo_hub POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/webui/index.html $<TARGET_FILE_DIR:hokuyo_hub>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/webui/app.js $<TARGET_FILE_DIR:hokuyo_hub>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/configs/default.yaml $<TARGET_FILE_DIR:hokuyo_hub>
)