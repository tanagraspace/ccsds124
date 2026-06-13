cmake_minimum_required(VERSION 3.14)

# Read version from VERSION file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" VERSION_CONTENT)
string(STRIP "${VERSION_CONTENT}" PROJECT_VERSION)

project(ccsds124
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "CCSDS 124.0-B-1 Compression Library"
    LANGUAGES CXX
)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(CCSDS124_NO_EXCEPTIONS "Disable exceptions for embedded use" OFF)
option(CCSDS124_NO_RTTI "Disable RTTI for embedded use" OFF)
option(CCSDS124_BUILD_TESTS "Build unit tests" ON)
option(CCSDS124_BUILD_CLI "Build CLI tool" ON)
option(CCSDS124_BUILD_BENCH "Build benchmarks" ON)
option(CCSDS124_COVERAGE "Enable code coverage" OFF)

# Compiler flags
add_compile_options(-Wall -Wextra -Wpedantic)

if(CCSDS124_NO_EXCEPTIONS)
    add_compile_options(-fno-exceptions)
    add_compile_definitions(CCSDS124_NO_EXCEPTIONS=1)
endif()

if(CCSDS124_NO_RTTI)
    add_compile_options(-fno-rtti)
    add_compile_definitions(CCSDS124_NO_RTTI=1)
endif()

if(CCSDS124_COVERAGE)
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

# Library sources
set(CCSDS124_SOURCES
    src/bitvector.cpp
    src/bitbuffer.cpp
    src/bitreader.cpp
    src/encoder.cpp
    src/decoder.cpp
    src/compressor.cpp
    src/decompressor.cpp
)

# Static library (named ccsds124_cxx to avoid conflict with C library)
add_library(ccsds124_cxx STATIC ${CCSDS124_SOURCES})
target_include_directories(ccsds124_cxx PUBLIC include)
target_compile_features(ccsds124_cxx PUBLIC cxx_std_17)

# CLI executable
if(CCSDS124_BUILD_CLI)
    add_executable(ccsds124_cli src/cli.cpp)
    target_link_libraries(ccsds124_cli PRIVATE ccsds124_cxx)
    set_target_properties(ccsds124_cli PROPERTIES OUTPUT_NAME ccsds124)
endif()

# Testing with Catch2 (single-header)
if(CCSDS124_BUILD_TESTS)
    enable_testing()

    # Fetch Catch2 v3
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0
    )
    FetchContent_MakeAvailable(Catch2)

    # Test executable
    add_executable(tests
        tests/test_bitvector.cpp
        tests/test_bitbuffer.cpp
        tests/test_bitreader.cpp
        tests/test_encoder.cpp
        tests/test_decoder.cpp
        tests/test_mask.cpp
        tests/test_compressor.cpp
        tests/test_decompressor.cpp
        tests/test_vectors.cpp
        tests/test_edge_cases.cpp
        tests/test_differential.cpp
    )
    target_link_libraries(tests PRIVATE ccsds124_cxx Catch2::Catch2WithMain)

    # Register tests
    include(CTest)
    include(Catch)
    catch_discover_tests(tests)
endif()

# Benchmarks
if(CCSDS124_BUILD_BENCH)
    add_executable(bench bench/bench.cpp)
    target_link_libraries(bench PRIVATE ccsds124_cxx)
endif()

# Install
install(TARGETS ccsds124_cxx
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/ccsds124 DESTINATION include)
