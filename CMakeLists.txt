cmake_minimum_required(VERSION 3.13)

project(codecrafters-http-server)

set(CMAKE_C_STANDARD 23) # Enable the C23 standard
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS
    "-Wall -Wextra -Werror -Wpointer-arith -Wundef -Wcast-qual -Wwrite-strings -Wshadow -Wfloat-equal -Wpedantic"
)
find_package(ZLIB REQUIRED)

file(GLOB_RECURSE SOURCE_FILES src/*.c src/*.h)
list(REMOVE_ITEM SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c)

add_library(http-server-lib STATIC ${SOURCE_FILES})
target_link_libraries(http-server-lib PUBLIC ZLIB::ZLIB)
target_compile_definitions(http-server-lib PUBLIC ZLIB_CONST=1)

add_executable(http-server src/main.c)
target_link_libraries(http-server PRIVATE http-server-lib)
