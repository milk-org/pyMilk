cmake_minimum_required(VERSION 3.17)

set(LIBNAME "ProcessInfoWrap")

project(${LIBNAME} LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# =======================================
# PYBIND11
# =======================================
set(PYBIND11_FINDPYTHON ON)
execute_process(
  COMMAND ${PYTHON_EXECUTABLE} -m pybind11 --cmakedir
  OUTPUT_VARIABLE pybind11_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
find_package(pybind11 REQUIRED HINTS ${pybind11_DIR})
pybind11_add_module(${LIBNAME} ${LIBNAME}.cpp)


# =======================================
# COVERAGE BUILD
# =======================================
if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
  target_compile_definitions(${LIBNAME} PRIVATE COVERAGE_BUILD=1)
  target_link_options(${LIBNAME} PRIVATE --coverage)
  target_link_libraries(${LIBNAME} PRIVATE gcov)
endif()

target_link_libraries(${LIBNAME} PUBLIC ProcessInfopyMilk)

install(TARGETS ${LIBNAME}
        EXPORT ${LIBNAME}Config
        ARCHIVE DESTINATION python
        LIBRARY DESTINATION python)
