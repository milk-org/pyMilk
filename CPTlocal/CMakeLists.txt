cmake_minimum_required(VERSION 3.10)

set(LIBNAME "CPTlocal")

message("")
message(" SRCNAME = ${SRCNAME} -> LIBNAME = ${LIBNAME}")

option(build_python_module "Compile Python Wrappers" OFF)
option(gtest_build_tests "Build Unit Tests" OFF)

project(${LIBNAME} LANGUAGES C)

# Version number
set ( VERSION_MAJOR 1 )
set ( VERSION_MINOR 00 )
set ( VERSION_PATCH 00 )
set ( VERSION_OPTION "" )
set ( PROJECT_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}" )
configure_file (
  "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}_config.h.in"
  "${PROJECT_SOURCE_DIR}//${PROJECT_NAME}_config.h"
  )
message(" VERSION = ${PROJECT_VERSION}")

set(CMAKE_C_FLAGS_DEBUG     "-O0 -g -Wall -Wextra -std=gnu17 -Wno-sign-compare")
# cmake -DCMAKE_BUILD_TYPE=Coverage
set(CMAKE_C_FLAGS_COVERAGE     "-O0 -g --coverage -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_FLAGS_COVERAGE     "-O0 -g --coverage -fprofile-arcs -ftest-coverage")
# cmake -DCMAKE_BUILD_TYPE=Release
set(CMAKE_C_FLAGS_RELEASE     "-Ofast -DNDEBUG -std=gnu17")

# Set a default build type if none was specified
set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}")
endif()

if(build_python_module)
  add_subdirectory(python_module)
endif()

########################################################################
#
# CONFIG.CMAKE
#
