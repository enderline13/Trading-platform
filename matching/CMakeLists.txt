cmake_minimum_required(VERSION 4.1)
project(Matching)

set(CMAKE_CXX_STANDARD 23)

set(HEADERS
        include/matching/matchingEngine.h
        include/matching/orderBook.h
)

set(SOURCES
        src/matchingEngine.cpp
        src/orderBook.cpp
)

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC include/)

target_link_libraries(${PROJECT_NAME} common)