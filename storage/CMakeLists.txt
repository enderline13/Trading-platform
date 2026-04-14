cmake_minimum_required(VERSION 4.1)
project(Storage)

set(CMAKE_CXX_STANDARD 23)

set(HEADERS
        include/storage/IOrderRepository.h
        include/storage/IAccountRepository.h
        include/storage/ITradeRepository.h
        include/storage/IUserRepository.h
)

set(SOURCES
        src/mysql_account_repository.cpp
        src/mysql_order_repository.cpp
        src/mysql_trade_repository.cpp
        src/mysql_user_repository.cpp
)

find_package(mysql-concpp REQUIRED)

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE mysql::concpp)

target_include_directories(${PROJECT_NAME} PUBLIC include/)

target_link_libraries(${PROJECT_NAME} PUBLIC common)