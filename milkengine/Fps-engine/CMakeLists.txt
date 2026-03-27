cmake_minimum_required(VERSION 3.17)

set(LIBNAME FpspyMilk)
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

# Link dependencies
target_link_libraries(${LIBNAME} PUBLIC ProcessInfopyMilk ImageStreamIOpyMilk rt pthread m)
    
install(TARGETS ${LIBNAME} EXPORT milkTargets DESTINATION lib)

file(GLOB HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
list(FILTER HEADER_FILES EXCLUDE REGEX "_internal")
install(FILES ${HEADER_FILES} DESTINATION include)
