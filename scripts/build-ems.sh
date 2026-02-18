#!/bin/bash

# Build script for EMS Java service

set -e

echo "=== Building EMS Java Service ==="
echo

# Check for Maven
if ! command -v mvn &> /dev/null; then
    echo "Error: Maven not found. Please install Maven."
    exit 1
fi

# Navigate to EMS directory
cd services/ems-java

# Build
echo "Building with Maven..."
mvn clean package -DskipTests

echo
echo "=== Build Complete ==="
echo
echo "Run EMS service:"
echo "  java -jar services/ems-java/target/ems-java-1.0.0.jar"
