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
add_subdirectory(common)
add_subdirectory(proto_generated)
enable_testing()
add_subdirectory(tests)

add_custom_target(build_all_services)
add_dependencies(build_all_services
        common
        Gateway
        Storage
        Core
        Matching
        Market_data
        proto_generated
)