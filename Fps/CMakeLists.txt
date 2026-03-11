cmake_minimum_required(VERSION 3.17)

set(LIBNAME "Fps")
message("")
message(" SRCNAME = ${SRCNAME} -> LIBNAME = ${LIBNAME}")

option(build_python_module "Compile Python Wrappers" OFF)

project(${LIBNAME} LANGUAGES C CXX)

file(GLOB SOURCES "*.c" "*.cpp")

# Core library (no TUI/ncurses dependency)
add_library(${LIBNAME} SHARED ${SOURCES})

# =======================================
# BUILD TYPE
# =======================================
# cmake -DCMAKE_BUILD_TYPE=Debug
# option: add -fanalyzer if gcc10+ (warning: this will take a while to run)
set(CMAKE_C_FLAGS_DEBUG     "-O0 -g -Wall -Wextra -std=gnu17 -Wno-sign-compare -Wl,--no-undefined")
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

message("CMAKE_BUILD_TYPE : ${CMAKE_BUILD_TYPE}")

# Include directories
target_include_directories(${LIBNAME} PRIVATE ${PROJECT_SOURCE_DIR})
target_include_directories(${LIBNAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
                                             $<INSTALL_INTERFACE:include/${SRCNAME}>
                                             $<INSTALL_INTERFACE:include>)

# Link dependencies
target_link_libraries(${LIBNAME} PUBLIC ImageStreamIO rt pthread m)

# =======================================
# COVERAGE BUILD
# =======================================
if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
  target_compile_definitions(${LIBNAME} PRIVATE COVERAGE_BUILD=1)
  target_link_options(${LIBNAME} PRIVATE --coverage)
  target_link_libraries(${LIBNAME} PRIVATE gcov)
endif()

if(build_python_module)
  add_subdirectory(python_module)
endif()


install(TARGETS ${LIBNAME} EXPORT ${LIBNAME}Targets DESTINATION lib)
install(FILES
    fps.h
    fps_globals.h
    DESTINATION include)
