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

add_compile_options(-Ofast)

if(build_python_module)
  add_subdirectory(python_module)
endif()

########################################################################
#
# CONFIG.CMAKE
#
