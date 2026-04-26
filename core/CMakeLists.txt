cmake_minimum_required(VERSION 4.1)
project(Core)

set(CMAKE_CXX_STANDARD 23)

find_package(nlohmann_json REQUIRED)

set(HEADERS
        include/core/tradingCore.h
        include/core/accountManager.h
        include/core/authManager.h
        include/core/core.h
        include/core/TransactionGuard.h
        include/core/adminManager.h
)

set(SOURCES
        src/tradingCore.cpp
        src/accountManager.cpp
        src/authManager.cpp
        src/adminManager.cpp
)

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC include/)

target_link_libraries(${PROJECT_NAME} PUBLIC common Market_data Matching Storage proto_generated)

target_link_libraries(${PROJECT_NAME} PUBLIC nlohmann_json::nlohmann_json)