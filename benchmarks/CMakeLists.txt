cmake_minimum_required(VERSION 4.1)
project(Benchmark)

set(CMAKE_CXX_STANDARD 23)

find_package(benchmark REQUIRED)

add_executable(${PROJECT_NAME}
        src/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE benchmark::benchmark_main)