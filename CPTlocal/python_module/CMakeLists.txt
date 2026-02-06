cmake_minimum_required(VERSION 3.10)

# Global option #
#
set(LIBNAME "CPTlocalWrap")

set(CMAKE_CXX_STANDARD 14)

project(${LIBNAME} LANGUAGES CXX)

# Set a default build type if none was specified
set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}")
endif()

message("CMAKE_BUILD_TYPE : ${CMAKE_BUILD_TYPE}")


if(NOT TARGET pybind11)
execute_process(COMMAND bash -c "${PYTHON_EXECUTABLE} -m pybind11 --includes"
OUTPUT_VARIABLE pybind11_inc)
execute_process(COMMAND bash -c "python3-config --extension-suffix"
OUTPUT_VARIABLE PYTHON_MODULE_EXTENSION)
string(REPLACE "-I" "" pybind11_inc ${pybind11_inc})
string(REPLACE " " ";" pybind11_inc ${pybind11_inc})
string(REGEX REPLACE "\n$" "" pybind11_inc "${pybind11_inc}")
string(REGEX REPLACE "\n$" "" PYTHON_MODULE_EXTENSION "${PYTHON_MODULE_EXTENSION}")

add_library(${LIBNAME} MODULE CacaoProcessTools.cpp)

target_compile_features(${LIBNAME} PUBLIC cxx_std_14)
target_include_directories(${LIBNAME} PUBLIC "${pybind11_inc}")
target_compile_options(${LIBNAME} PUBLIC "-Wno-deprecated-declarations")
set_target_properties(${LIBNAME}
PROPERTIES PREFIX
"${PYTHON_MODULE_PREFIX}"
SUFFIX
"${PYTHON_MODULE_EXTENSION}")

else()
pybind11_add_module(${LIBNAME} CacaoProcessTools.cpp)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
  target_link_options(${LIBNAME} PRIVATE --coverage)
  target_link_libraries(${LIBNAME} PRIVATE gcov)
endif()

message("This is MILK_INSTALLDIR=$ENV{MILK_INSTALLDIR}")
target_include_directories(${LIBNAME} PRIVATE "$ENV{MILK_INSTALLDIR}/include/" "$ENV{MILK_INSTALLDIR}/include/CommandLineInterface")
target_link_directories(${LIBNAME} PRIVATE "$ENV{MILK_INSTALLDIR}/lib/")
target_link_libraries(${LIBNAME} PRIVATE CLIcore ImageStreamIO milkCOREMODiofits milkCOREMODmemory milkCOREMODtools)

install(TARGETS ${LIBNAME}
        EXPORT ${LIBNAME}Config
        ARCHIVE DESTINATION python
        LIBRARY DESTINATION python)
