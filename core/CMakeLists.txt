cmake_minimum_required(VERSION 4.1)
project(Core)

set(CMAKE_CXX_STANDARD 23)

set(HEADERS
        include/core/test.h
)

set(SOURCES
        src/main.cpp
)

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC include/core)

target_link_libraries(${PROJECT_NAME} PRIVATE Market_data Matching Storage)