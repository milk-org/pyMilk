project(pyMilk)
cmake_minimum_required(VERSION 3.8)

set(build_python_module ON CACHE BOOL "Build ImageStreamIO with python extension" FORCE)

find_package(CUDA)

if(CUDA_FOUND)
	set(USE_CUDA ON CACHE BOOL "Build ImageStreamIO with CUDA" FORCE)
endif(CUDA_FOUND)

add_subdirectory(ImageStreamIO)
