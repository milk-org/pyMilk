cmake_minimum_required(VERSION 3.17)

project(pyMilk)

set(build_python_module ON CACHE BOOL "Build extensions with python extension" FORCE)

include(CheckLanguage)
check_language(CUDA)

if(CMAKE_CUDA_COMPILER)
	enable_language(CUDA)
	set(USE_CUDA ON CACHE BOOL "Build ImageStreamIO with CUDA" FORCE)
endif()

set(USE_NCURSES OFF CACHE BOOL "Build with ncurses" FORCE)
set(USE_CLI OFF CACHE BOOL "Build with CLI" FORCE)
set(USE_STATIC_LTO OFF CACHE BOOL "Build with LTO" FORCE)

# =======================================
# BUILD TYPE
# =======================================
# cmake -DCMAKE_BUILD_TYPE=Debug
# option: add -fanalyzer if gcc10+ (warning: this will take a while to run)
set(CMAKE_C_FLAGS_DEBUG     "-O0 -g -Wall -Wextra -std=gnu17 -Wno-sign-compare")
# cmake -DCMAKE_BUILD_TYPE=Coverage
set(CMAKE_C_FLAGS_COVERAGE     "-O0 -g --coverage -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_FLAGS_COVERAGE     "-O0 -g --coverage -fprofile-arcs -ftest-coverage")
# cmake -DCMAKE_BUILD_TYPE=Release
set(CMAKE_C_FLAGS_RELEASE     "-Ofast -DNDEBUG -std=gnu17")

add_subdirectory(ImageStreamIO)
add_subdirectory(ProcessInfo-engine)
add_subdirectory(Fps-engine)
add_subdirectory(pinfo_python_module)
add_subdirectory(fps_python_module)
#add_subdirectory(ProcessInfo)
#add_subdirectory(Fps)
