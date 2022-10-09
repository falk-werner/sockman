# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

cmake_minimum_required (VERSION 3.10)
project(sockman VERSION 1.0.0)

option(WITHOUT_TESTS    "disable unit tests" OFF)
option(WITHOUT_EXAMPLES "disable examples"   OFF)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)


add_library(sockman STATIC src/sockman/manager.cpp)
target_include_directories(sockman PUBLIC include)
target_include_directories(sockman PRIVATE src)


if(NOT WITHOUT_TESTS)

find_package(PkgConfig REQUIRED)
include (CTest)

pkg_check_modules(GTEST REQUIRED gtest_main)
include(GoogleTest)
pkg_check_modules(GMOCK REQUIRED gmock)

add_executable(alltests
    test-src/sockman/test_manager.cpp
)

target_include_directories(alltests PRIVATE
    test-src
    src
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

target_compile_options(alltests PRIVATE
    ${GTEST_CFLAGS}
    ${GTEST_CFLAGS_OTHER}
    ${GMOCK_CFLAGS}
    ${GMOCK_CFLAGS_OTHER}
)

target_link_libraries(alltests PRIVATE
    sockman
    ${GTEST_LIBRARIES}
    ${GMOCK_LIBRARIES}
)

enable_testing()
gtest_discover_tests(alltests TEST_PREFIX alltests:)

add_custom_target(memcheck
    valgrind ./alltests)

endif(NOT WITHOUT_TESTS)


if(NOT WITHOUT_EXAMPLES)

add_executable(echo_server example/echo_server.cpp)
target_link_libraries(echo_server PRIVATE sockman)

add_executable(echo_client example/echo_client.cpp)
target_link_libraries(echo_client PRIVATE sockman)


endif(NOT WITHOUT_EXAMPLES)