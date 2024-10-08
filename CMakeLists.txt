cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

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

set(CMAKE_CXX_STANDARD 23)

find_package(Boost REQUIRED COMPONENTS program_options system)
find_path(PARALLEL_HASHMAP_INCLUDE_DIRS "parallel_hashmap/btree.h")
find_package(xxHash CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

add_executable(UCPP
    "src/main.cpp"
    "src/preprocessor.cpp"
    "src/lexer.cpp"
    "src/file_service.cpp")
target_include_directories(UCPP PRIVATE Boost_INCLUDE_DIR ${PARALLEL_HASHMAP_INCLUDE_DIRS} xxHash_INCLUDE_DIR)
target_link_libraries(UCPP PRIVATE Boost::boost Boost::program_options Boost::system)

add_executable(LexerTest
    "src/lexer.cpp"
    "src/lexer_test.cpp")
target_link_libraries(LexerTest PRIVATE Catch2::Catch2 Catch2::Catch2WithMain)
add_test(NAME lexer_test COMMAND LexerTest)