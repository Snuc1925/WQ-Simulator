#!/bin/bash

# Build script for WQ-Simulator C++ services

set -e

echo "=== Building WQ-Simulator ==="
echo

# Check for required tools
echo "Checking dependencies..."
if ! command -v cmake &> /dev/null; then
    echo "Error: cmake not found. Please install cmake."
    exit 1
fi

if ! command -v protoc &> /dev/null; then
    echo "Error: protoc not found. Please install protobuf compiler."
    exit 1
fi

echo "Dependencies OK"
echo

# Create build directory
BUILD_DIR="build"
if [ -d "$BUILD_DIR" ]; then
    echo "Build directory exists, cleaning..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Run CMake
echo "Running CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo
echo "Building services..."
make -j$(nproc)

echo
echo "=== Build Complete ==="
echo
echo "Executables:"
echo "  - data-feed-handler-server"
echo "  - alpha-engine-server"
echo "  - signal-aggregator-server"
echo "  - risk-guardian-server"
echo
echo "Run services from build directory:"
echo "  cd build"
echo "  ./services/data-feed-handler/data-feed-handler-server"
