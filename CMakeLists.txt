cmake_minimum_required(VERSION 3.17)

project(pyMilk)

set(build_python_module ON CACHE BOOL "Build extensions with python extension" FORCE)

include(CheckLanguage)
check_language(CUDA)

if(CMAKE_CUDA_COMPILER)
	message("YES CUDA.")
	enable_language(CUDA)
	set(USE_CUDA ON CACHE BOOL "Build ImageStreamIO with CUDA" FORCE)
endif()

add_subdirectory(ImageStreamIO)
add_subdirectory(ProcessInfo)
add_subdirectory(Fps)
