cmake_minimum_required(VERSION 4.1)
project(Platform)

set(CMAKE_CXX_STANDARD 23)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_link_options("-fuse-ld=lld")
endif()

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mysql-connector")

add_subdirectory(client)
add_subdirectory(storage)
add_subdirectory(gateway)
add_subdirectory(benchmarks)
add_subdirectory(core)
add_subdirectory(matching)
add_subdirectory(market_data)
enable_testing()
add_subdirectory(tests)