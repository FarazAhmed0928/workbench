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
