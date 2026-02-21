cmake_minimum_required(VERSION 4.1)
project(Gateway)

set(CMAKE_CXX_STANDARD 23)

find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

add_library(${PROJECT_NAME}
        src/main.cpp
)

file(GLOB PROTO_FILES "${CMAKE_SOURCE_DIR}/proto/*.proto")
message(STATUS "Found proto files: ${PROTO_FILES}")

protobuf_generate(
        TARGET ${PROJECT_NAME}
        LANGUAGE cpp
        PROTOS ${PROTO_FILES}
        IMPORT_DIRS "${CMAKE_SOURCE_DIR}/proto"
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${protobuf_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE gRPC::grpc++ gRPC::grpc protobuf::protobuf)