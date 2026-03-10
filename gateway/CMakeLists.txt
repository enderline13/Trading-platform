cmake_minimum_required(VERSION 4.1)
project(Gateway)

set(CMAKE_CXX_STANDARD 23)

find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(jwt-cpp REQUIRED)

add_executable(${PROJECT_NAME}
        src/main.cpp
)

file(GLOB PROTO_FILES "${CMAKE_SOURCE_DIR}/proto/*.proto")

protobuf_generate(
        TARGET ${PROJECT_NAME}
        LANGUAGE cpp
        PROTOS ${PROTO_FILES}
        IMPORT_DIRS "${CMAKE_SOURCE_DIR}/proto"
)

protobuf_generate(
        TARGET ${PROJECT_NAME}
        LANGUAGE grpc
        GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
        PLUGIN "protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
        PROTOS ${PROTO_FILES}
        IMPORT_DIRS "${CMAKE_SOURCE_DIR}/proto"
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${protobuf_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE gRPC::grpc++ gRPC::grpc protobuf::protobuf jwt-cpp::jwt-cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE Core)