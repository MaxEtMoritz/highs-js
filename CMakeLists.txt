cmake_minimum_required(VERSION 3.15...3.27)
project(HiGHS_JS VERSION 1.0.0 LANGUAGES CXX)

add_subdirectory(HiGHS)

include(${HIGHS_BINARY_DIR}/highs-config.cmake)

add_library(highs_bindings STATIC src/HiGHS_Bindings.cpp)

include_directories(${HIGHS_INCLUDE_DIRS})

target_link_libraries(highs_bindings libhighs)