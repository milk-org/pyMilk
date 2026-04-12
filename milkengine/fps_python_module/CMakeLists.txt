cmake_minimum_required(VERSION 3.17)

set(LIBNAME "FpsWrap")

project(${LIBNAME} LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# =======================================
# NANOBIND
# =======================================
find_package(Python 3.9 COMPONENTS Interpreter Development.Module REQUIRED)
execute_process(
  COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE nanobind_ROOT)
find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(${LIBNAME} ${LIBNAME}.cpp)

# =======================================
# COVERAGE BUILD
# =======================================
if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
  target_compile_definitions(${LIBNAME} PRIVATE COVERAGE_BUILD=1)
  target_link_options(${LIBNAME} PRIVATE --coverage)
  target_link_libraries(${LIBNAME} PRIVATE gcov)
endif()

target_link_libraries(${LIBNAME} PUBLIC FpspyMilk)

install(TARGETS ${LIBNAME}
        EXPORT ${LIBNAME}Config
        ARCHIVE DESTINATION python
        LIBRARY DESTINATION python)
