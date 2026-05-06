cmake_minimum_required(VERSION 4.1)
project(Market_data)

set(CMAKE_CXX_STANDARD 23)

set(HEADERS
        include/market_data/CandleAggregator.h
        include/market_data/InstrumentData.h
)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE include/)

target_link_libraries(${PROJECT_NAME} INTERFACE common)