cmake_minimum_required(VERSION 3.15)

set(SERVICE_NAME data-feed-handler)

# Source files
set(SOURCES
    src/data_types.cpp
    src/data_feed_handler.cpp
)

# Create library
add_library(${SERVICE_NAME} STATIC ${SOURCES})

target_include_directories(${SERVICE_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${SERVICE_NAME}
    PUBLIC
        wq_proto
        Threads::Threads
)

# Main executable
add_executable(${SERVICE_NAME}-server src/main.cpp)
target_link_libraries(${SERVICE_NAME}-server
    PRIVATE
        ${SERVICE_NAME}
        wq_proto
)
