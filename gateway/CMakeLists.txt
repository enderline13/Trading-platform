cmake_minimum_required(VERSION 4.1)
project(Gateway)

set(CMAKE_CXX_STANDARD 23)

find_package(jwt-cpp REQUIRED)

add_executable(${PROJECT_NAME}
        src/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE jwt-cpp::jwt-cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE Core proto_generated)