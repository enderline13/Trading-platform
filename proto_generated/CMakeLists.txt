cmake_minimum_required(VERSION 4.1)
project(proto_generated)

set(CMAKE_CXX_STANDARD 23)

find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf REQUIRED)

set(PROTO_DIR "${CMAKE_SOURCE_DIR}/proto")
set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GEN_DIR})

file(GLOB PROTO_FILES CONFIGURE_DEPENDS "${PROTO_DIR}/*.proto")

protobuf_generate(
        OUT_VAR PROTO_SRCS
        LANGUAGE cpp
        PROTOS ${PROTO_FILES}
        IMPORT_DIRS ${PROTO_DIR}
        PROTOC_OUT_DIR ${GEN_DIR}
)

protobuf_generate(
        OUT_VAR GRPC_SRCS
        LANGUAGE grpc
        GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
        PLUGIN "protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
        PROTOS ${PROTO_FILES}
        IMPORT_DIRS ${PROTO_DIR}
        PROTOC_OUT_DIR ${GEN_DIR}
)

add_library(${PROJECT_NAME} STATIC
        ${PROTO_SRCS}
        ${GRPC_SRCS}
)

target_link_libraries(${PROJECT_NAME}
        PUBLIC
        gRPC::grpc++ gRPC::grpc protobuf::protobuf
)

target_include_directories(${PROJECT_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${GEN_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)