cmake_minimum_required(VERSION 3.14)
project(Entable LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Benchmark dependency strategy:
# - OFF (default): fetch google/benchmark from GitHub
# - ON: use preinstalled benchmark package via find_package
option(ENTABLE_USE_SYSTEM_BENCHMARK "Use installed benchmark package instead of FetchContent" OFF)

# Testing option
option(ENTABLE_BUILD_TESTS "Build Catch2 tests" ON)

include(FetchContent)

if(ENTABLE_USE_SYSTEM_BENCHMARK)
  find_package(benchmark CONFIG REQUIRED)
else()
  FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.5
  )
  set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(benchmark)
endif()

# Catch2 for testing
if(ENTABLE_BUILD_TESTS)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.13.0
  )
  FetchContent_MakeAvailable(Catch2)
endif()

# Header-only Entable: create interface library so benchmarks can include it
add_library(entable INTERFACE)
target_include_directories(entable INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Treat warnings as errors for all targets
# MSVC: /W4 for warnings, /WX to treat as errors
# Other compilers: -Wall -Wextra -Werror
target_compile_options(entable INTERFACE
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror>
)

# Benchmark executable: ChunkedArray vs std::vector (normal types only)
add_executable(chunked_array_benchmarks benchmarks/vector_benchmarks.cpp)
target_link_libraries(chunked_array_benchmarks PRIVATE entable benchmark::benchmark benchmark::benchmark_main)

# Benchmark executable: SoA (Registry) vs AoS (aggregate vector)
add_executable(entable_benchmarks benchmarks/soa_aos_benchmarks.cpp)
target_link_libraries(entable_benchmarks PRIVATE entable benchmark::benchmark benchmark::benchmark_main)

# Test executable: Catch2 tests for ChunkedArray
if(ENTABLE_BUILD_TESTS)
  add_executable(chunked_array_tests tests/ChunkedArray_tests.cpp)
  target_link_libraries(chunked_array_tests PRIVATE entable Catch2::Catch2WithMain)

  # Test executable: Catch2 tests for Entable Registry
  add_executable(entable_tests tests/Entable_tests.cpp)
  target_link_libraries(entable_tests PRIVATE entable Catch2::Catch2WithMain)

  enable_testing()
  add_test(NAME chunked_array_tests COMMAND chunked_array_tests)
  add_test(NAME entable_tests COMMAND entable_tests)
endif()
