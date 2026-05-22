#!/bin/bash
PORT=${1:-8080}

if [ ! -f "build/mysqlite-web" ]; then
    echo "Building project..."
    ./build.sh
fi

./build/mysqlite-web --port $PORT
