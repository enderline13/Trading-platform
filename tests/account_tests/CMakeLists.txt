cmake_minimum_required(VERSION 4.1)
project(Account_tests)

set(CMAKE_CXX_STANDARD 23)

find_package(GTest REQUIRED)

add_executable(${PROJECT_NAME}
        src/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE GTest::gtest GTest::gtest_main Storage Core)