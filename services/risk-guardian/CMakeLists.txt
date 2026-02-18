cmake_minimum_required(VERSION 3.15)

set(SERVICE_NAME risk-guardian)

# Source files
set(SOURCES
    src/risk_checks.cpp
    src/risk_guardian.cpp
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
