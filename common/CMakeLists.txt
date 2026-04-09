cmake_minimum_required(VERSION 4.1)
project(common)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE include/)