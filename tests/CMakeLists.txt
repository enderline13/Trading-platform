cmake_minimum_required(VERSION 4.1)
project(Testing)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(core_tests)
add_subdirectory(market_data_tests)
add_subdirectory(matching_tests)
add_subdirectory(storage_tests)
add_subdirectory(trading_core_tests)
add_subdirectory(auth_tests)
add_subdirectory(account_tests)