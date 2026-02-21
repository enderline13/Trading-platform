cmake_minimum_required(VERSION 4.1)
project(Storage)

set(CMAKE_CXX_STANDARD 23)

find_package(mysql-concpp REQUIRED)

add_library(${PROJECT_NAME}
        src/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE mysql::concpp)