cmake_minimum_required(VERSION 4.1)
project(Gateway)

set(CMAKE_CXX_STANDARD 23)

add_executable(${PROJECT_NAME}
        src/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE Core proto_generated common)