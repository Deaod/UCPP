cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(UCPP)

find_package(Boost 1.64.0 COMPONENTS program_options system)
find_path(PARALLEL_HASHMAP_INCLUDE_DIRS "parallel_hashmap/btree.h")
find_package(xxHash CONFIG REQUIRED)

add_executable(UCPP "main.cpp" "preprocessor.h" "preprocessor.cpp" "types.h" "lexer.h" "lexer.cpp")
set_target_properties(UCPP PROPERTIES CXX_STANDARD 17)
target_include_directories(UCPP PRIVATE Boost_INCLUDE_DIR ${PARALLEL_HASHMAP_INCLUDE_DIRS} xxHash_INCLUDE_DIR)
target_link_libraries(UCPP Boost::program_options Boost::system)
