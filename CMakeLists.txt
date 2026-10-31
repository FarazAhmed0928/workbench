cmake_minimum_required(VERSION 3.16)
project(UnifiedWorkbench VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Core library: no UI or transport code in here.
add_library(wb_core STATIC
  src/core/fea1d.cpp
  src/core/stl.cpp
  src/core/nodal_csv.cpp
  src/core/thread_pool.cpp
  src/core/batch.cpp
  src/core/sysinfo.cpp
  src/core/benchmark.cpp
  src/core/backprop.cpp
  src/core/svgplot.cpp
  src/core/devices.cpp
)
target_include_directories(wb_core PUBLIC src/core)
if(MSVC)
  target_compile_options(wb_core PRIVATE /W4)
else()
  target_compile_options(wb_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_executable(test_fea tests/test_fea.cpp)
target_link_libraries(test_fea PRIVATE wb_core)
add_test(NAME fea COMMAND test_fea)

add_executable(gen_examples tools/gen_examples.cpp)
target_link_libraries(gen_examples PRIVATE wb_core)

add_executable(test_parsers tests/test_parsers.cpp)
target_link_libraries(test_parsers PRIVATE wb_core)
add_test(NAME parsers COMMAND test_parsers WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(wb_core PUBLIC Threads::Threads)
if(WIN32)
  target_link_libraries(wb_core PUBLIC advapi32)
endif()
# Record the optimisation flags actually used, for the benchmark report.
string(TOUPPER "${CMAKE_BUILD_TYPE}" WB_BT)
set(WB_FLAGS_STR "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${WB_BT}}")
string(STRIP "${WB_FLAGS_STR}" WB_FLAGS_STR)
set_source_files_properties(src/core/sysinfo.cpp PROPERTIES COMPILE_DEFINITIONS "WB_BUILD_FLAGS=\"${WB_FLAGS_STR}\"")

add_executable(bench tools/bench.cpp)
target_link_libraries(bench PRIVATE wb_core)

add_executable(test_batch tests/test_batch.cpp)
target_link_libraries(test_batch PRIVATE wb_core)
add_test(NAME batch COMMAND test_batch)

add_executable(gen_results tools/gen_results.cpp)
target_link_libraries(gen_results PRIVATE wb_core)

add_executable(test_backprop tests/test_backprop.cpp)
target_link_libraries(test_backprop PRIVATE wb_core)
add_test(NAME backprop COMMAND test_backprop)

add_executable(test_devices tests/test_devices.cpp)
target_link_libraries(test_devices PRIVATE wb_core)
add_test(NAME devices COMMAND test_devices)
