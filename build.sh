#!/bin/bash
# build.sh

set -e

echo "🔧 Building MySQLite..."

# Проверяем CMake
if ! command -v cmake &> /dev/null; then
    echo "❌ CMake not found. Installing..."
    if command -v apt &> /dev/null; then
        sudo apt update && sudo apt install -y cmake g++
    elif command -v brew &> /dev/null; then
        brew install cmake
    else
        echo "Please install CMake manually"
        exit 1
    fi
fi

# Собираем
mkdir -p build
cd build
cmake ..
make -j$(nproc)

echo "✅ Build complete!"
echo "📁 Library: build/libdblib.a"
echo "🚀 Server:  build/mysqld"