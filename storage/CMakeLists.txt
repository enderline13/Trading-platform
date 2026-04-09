cmake_minimum_required(VERSION 4.1)
project(Storage)

set(CMAKE_CXX_STANDARD 23)

set(HEADERS
        include/storage/storage.h
)

set(SOURCES
        src/storage.cpp
)

find_package(mysql-concpp REQUIRED)

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE mysql::concpp)

target_include_directories(${PROJECT_NAME} PUBLIC include/storage)

target_link_libraries(${PROJECT_NAME} PUBLIC common)