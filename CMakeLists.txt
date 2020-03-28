cmake_minimum_required(VERSION 3.1)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES  ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/;/usr/local/")

# Enforce the no in-source builds rule early, otherwise it still generates some
# junk in the top-level directory
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(SEND_ERROR "In-source builds are not allowed.")
endif()

# Set std=cpp14 for all projects
set (CMAKE_CXX_STANDARD 14)

set (GCC_COVERAGE_COMPILE_FLAGS "-Wall -Wextra -Wno-unused-parameter")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS} ${GCC_OPTIMIZATION}")

if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gdwarf-4 -g3 -g -rdynamic")
    set (GCC_OPTIMIZATION "-Og")
else()
    set (GCC_OPTIMIZATION "-O3")
endif()

project(fpga_programmer C CXX)

include_directories(include)

# Need to locate libusb
find_package(PkgConfig)
pkg_check_modules(PC_LIBUSB REQUIRED libusb-1.0)
include_directories(${PC_LIBUSB_INCLUDE_DIRS})

add_executable(faff
    src/main.cpp
    src/cmdline.cpp
    )
target_link_libraries(faff
    ${PC_LIBUSB_LIBRARIES}
    )
