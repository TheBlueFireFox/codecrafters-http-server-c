# ------------------------------ SETUP ------------------------------
cmake_minimum_required(VERSION 3.21)

project(codecrafters-http-server)

option(HASHMAP_ENABLE_STATS "Collect hashmap operation statistics" OFF)

set(CMAKE_C_STANDARD 23) # Enable the C23 standard
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_FLAGS " \
    -Wall -Wextra -Werror -Wpointer-arith -Wundef -Wcast-qual -Wwrite-strings \
    -Wshadow -Wfloat-equal -Wpedantic \
    "
)

if(LOCAL)
    # set(CMAKE_C_CLANG_TIDY
    #     clang-tidy
    #     --use-color
    #     --fix
    # )
    # set(CMAKE_CXX_CLANG_TIDY
    #     clang-tidy
    #     --use-color
    #     --fix
    # )
endif()

# ------------------------------ GTEST ------------------------------

if(LOCAL)
    find_package(GTest CONFIG REQUIRED)
    find_package(benchmark CONFIG REQUIRED)
endif()

find_package(ZLIB REQUIRED)

# ------------------------------ BUILD ------------------------------

add_library(
  http-server-lib STATIC
  src/client.c
  src/hashmap.c
  src/hashmap_hasher.c
  src/http.c
  src/queue.c
  src/routes.c
  src/server.c
  src/thread.c
  src/utils.c
  src/vector.c
)

target_include_directories(http-server-lib PUBLIC src)

target_link_libraries(http-server-lib PUBLIC ZLIB::ZLIB)
target_compile_definitions(http-server-lib PUBLIC ZLIB_CONST=1)

if(HASHMAP_ENABLE_STATS)
    target_compile_definitions(http-server-lib PUBLIC HASHMAP_ENABLE_STATS=1)
endif()

add_executable(http-server src/main.c)
target_link_libraries(http-server PRIVATE http-server-lib)

# ----------------------------- TESTS / BENCHMARKS -----------------------------

if(LOCAL)
    enable_testing()

    add_executable(http-server-test
        test/hashmap.cpp
        test/queue.cpp
        test/vector.cpp
    )

    target_link_libraries(http-server-test
        PRIVATE
            http-server-lib
            GTest::gtest_main
    )
    target_compile_options(http-server-test PRIVATE
        -g
        -fno-omit-frame-pointer
    )

    include(GoogleTest)
    gtest_discover_tests(http-server-test)

    # -------------------------- BENCHMARKS --------------------------

    add_executable(http-server-benchmark
        benchmark/hashmap_bench.cpp
    )

    target_link_libraries(http-server-benchmark
        PRIVATE
            http-server-lib
            benchmark::benchmark_main
    )
    target_compile_options(http-server-benchmark PRIVATE
        -g
        -fno-omit-frame-pointer
    )
endif()
