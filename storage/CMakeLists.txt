cmake_minimum_required(VERSION 4.1)
project(Storage)

set(CMAKE_CXX_STANDARD 23)

set(HEADERS
        include/storage/IOrderRepository.h
        include/storage/IAccountRepository.h
        include/storage/ITradeRepository.h
        include/storage/IUserRepository.h
        include/storage/IInstrumentRepository.h
        include/storage/DatabaseManager.h
        include/storage/utils.h
)

find_package(mysql-concpp REQUIRED)

add_library(${PROJECT_NAME} INTERFACE)

target_link_libraries(${PROJECT_NAME} INTERFACE mysql::concpp-jdbc)

target_include_directories(${PROJECT_NAME} INTERFACE include/)

target_link_libraries(${PROJECT_NAME} INTERFACE common)