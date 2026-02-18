cmake_minimum_required(VERSION 3.15)
project(WQSimulator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3")

# Find required packages
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# Proto compilation
set(PROTO_PATH "${CMAKE_SOURCE_DIR}/proto")
set(GENERATED_PROTO_PATH "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_PROTO_PATH})

set(PROTO_FILES
    "${PROTO_PATH}/market_data.proto"
    "${PROTO_PATH}/alpha_signals.proto"
    "${PROTO_PATH}/portfolio.proto"
    "${PROTO_PATH}/risk.proto"
)

# Generate protobuf and gRPC sources
set(PROTO_SRCS)
set(PROTO_HDRS)
set(GRPC_SRCS)
set(GRPC_HDRS)

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    
    list(APPEND PROTO_SRCS "${GENERATED_PROTO_PATH}/${PROTO_NAME}.pb.cc")
    list(APPEND PROTO_HDRS "${GENERATED_PROTO_PATH}/${PROTO_NAME}.pb.h")
    list(APPEND GRPC_SRCS "${GENERATED_PROTO_PATH}/${PROTO_NAME}.grpc.pb.cc")
    list(APPEND GRPC_HDRS "${GENERATED_PROTO_PATH}/${PROTO_NAME}.grpc.pb.h")
    
    add_custom_command(
        OUTPUT "${GENERATED_PROTO_PATH}/${PROTO_NAME}.pb.cc"
               "${GENERATED_PROTO_PATH}/${PROTO_NAME}.pb.h"
               "${GENERATED_PROTO_PATH}/${PROTO_NAME}.grpc.pb.cc"
               "${GENERATED_PROTO_PATH}/${PROTO_NAME}.grpc.pb.h"
        COMMAND protobuf::protoc
        ARGS --grpc_out=${GENERATED_PROTO_PATH}
             --cpp_out=${GENERATED_PROTO_PATH}
             --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
             -I${PROTO_PATH}
             ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating protobuf/gRPC code for ${PROTO_NAME}"
    )
endforeach()

# Proto library
add_library(wq_proto STATIC ${PROTO_SRCS} ${GRPC_SRCS})
target_link_libraries(wq_proto
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
        gRPC::grpc++_reflection
)
target_include_directories(wq_proto PUBLIC ${GENERATED_PROTO_PATH})

# Add subdirectories for each service
add_subdirectory(services/data-feed-handler)
add_subdirectory(services/alpha-engine)
add_subdirectory(services/signal-aggregator)
add_subdirectory(services/risk-guardian)

# Common utilities library
add_library(wq_common INTERFACE)
target_include_directories(wq_common INTERFACE ${CMAKE_SOURCE_DIR}/common/include)
