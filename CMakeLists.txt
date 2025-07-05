# ------------------------------ SETUP ------------------------------
cmake_minimum_required(VERSION 3.21)

project(codecrafters-http-server)

set(CMAKE_C_STANDARD 23) # Enable the C23 standard
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 23) # Enable the C23 standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS
    "-Wall -Wextra -Werror -Wpointer-arith -Wundef -Wcast-qual -Wwrite-strings -Wshadow -Wfloat-equal -Wpedantic"
)

# ------------------------------ GTEST ------------------------------

if(LOCAL)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    DOWNLOAD_EXTRACT_TIMESTAMP 1
  )

  # For Windows: Prevent overriding the parent project's compiler/linker
  # settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

endif()

find_package(ZLIB REQUIRED)

# ------------------------------ BUILD ------------------------------

file(GLOB_RECURSE SOURCE_FILES src/*.c src/*.h)
list(REMOVE_ITEM SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c)

add_library(http-server-lib STATIC ${SOURCE_FILES})
target_include_directories(http-server-lib PUBLIC src)

target_link_libraries(http-server-lib PUBLIC ZLIB::ZLIB)
target_compile_definitions(http-server-lib PUBLIC ZLIB_CONST=1)

add_executable(http-server src/main.c)
target_link_libraries(http-server PRIVATE http-server-lib)

# ------------------------------ TESTS ------------------------------

if(LOCAL)
  enable_testing()

  add_executable(http-server-test test/queue.cpp)
  target_link_libraries(http-server-test GTest::gtest_main http-server-lib)

  include(GoogleTest)
  gtest_discover_tests(http-server-test)
endif()
