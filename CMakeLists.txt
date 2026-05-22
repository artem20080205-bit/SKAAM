cmake_minimum_required(VERSION 3.10)
project(MySQLite)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Библиотека
add_library(dblib STATIC
    src/api/database.cpp
    src/storage/storage.cpp
    src/commands/executor.cpp
)

target_include_directories(dblib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Веб сервер (только он)
add_executable(mysqlite-web
    src/http_server.cpp
)

target_link_libraries(mysqlite-web dblib pthread)