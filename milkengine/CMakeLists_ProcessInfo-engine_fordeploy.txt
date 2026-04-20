cmake_minimum_required(VERSION 3.17)

set(LIBNAME ProcessInfopyMilk)
project(${LIBNAME})


file(GLOB SOURCES "*.c" "*.cpp")
list(FILTER SOURCES EXCLUDE REGEX "milk-")

# Core library (no TUI/ncurses dependency)
add_library(${LIBNAME} SHARED ${SOURCES})

# Include directories
target_include_directories(${LIBNAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# =======================================
# COVERAGE BUILD
# =======================================
if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
  target_compile_definitions(${LIBNAME} PRIVATE COVERAGE_BUILD=1)
  target_link_options(${LIBNAME} PRIVATE --coverage)
  target_link_libraries(${LIBNAME} PRIVATE gcov)
endif()

# Link dependencies
target_link_libraries(${LIBNAME} PUBLIC ImageStreamIOpyMilk rt pthread m)

install(TARGETS ${LIBNAME} EXPORT milkTargets DESTINATION lib)

file(GLOB HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
list(FILTER HEADER_FILES EXCLUDE REGEX "_internal")
install(FILES ${HEADER_FILES} DESTINATION include)
