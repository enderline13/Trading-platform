cmake_minimum_required(VERSION 4.1)
project(Market_data)

set(CMAKE_CXX_STANDARD 23)

set(HEADERS
        include/market_data/marketData.h
)

set(SOURCES
        src/marketData.cpp
)

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC include/)

target_link_libraries(${PROJECT_NAME} common)