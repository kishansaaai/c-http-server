cmake_minimum_required(VERSION 3.10)
project(HttpServer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# We are targeting WSL/Linux
add_executable(http_server
    src/main.c
    src/server.c
)
