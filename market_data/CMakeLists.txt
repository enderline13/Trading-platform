cmake_minimum_required(VERSION 4.1)
project(Market_data)

set(CMAKE_CXX_STANDARD 23)

set(HEADERS
        include/market_data/test.h
)

set(SOURCES
        src/main.cpp
)

add_library(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE include/market_data/matching)