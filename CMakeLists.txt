cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

set(CMAKE_TOOLCHAIN_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

if(WIN32)
    set(VCPKG_TARGET_TRIPLET
        "x64-windows-static"
        CACHE STRING "Vcpkg target triplet")
endif()

project(UCPP)
enable_testing()

find_package(Boost REQUIRED COMPONENTS program_options system)
find_path(PARALLEL_HASHMAP_INCLUDE_DIRS "parallel_hashmap/btree.h")
find_package(xxHash CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

add_executable(UCPP
    "main.cpp"
    "preprocessor.h"
    "preprocessor.cpp"
    "types.h"
    "lexer.h"
    "lexer.cpp"
    "scope_guard.h"
    "parser.cpp" "decoder.cpp")
set_target_properties(UCPP PROPERTIES CXX_STANDARD 23)
target_include_directories(UCPP PRIVATE Boost_INCLUDE_DIR ${PARALLEL_HASHMAP_INCLUDE_DIRS} xxHash_INCLUDE_DIR)
target_link_libraries(UCPP PRIVATE Boost::boost Boost::program_options Boost::system)

add_executable(LexerTest
    "lexer.h"
    "lexer.cpp"
    "lexer_test.cpp")
set_target_properties(LexerTest PROPERTIES CXX_STANDARD 23)
target_link_libraries(LexerTest PRIVATE GTest::gtest GTest::gtest_main)
add_test(NAME lexer_test COMMAND LexerTest)
