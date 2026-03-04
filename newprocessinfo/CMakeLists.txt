cmake_minimum_required(VERSION 3.17)

set(LIBNAME "milkprocessinfo")

project(${LIBNAME} LANGUAGES C)

file(GLOB SOURCES "*.c")

# Core library (no TUI/ncurses dependency)
add_library(milkprocessinfo SHARED ${SOURCES})

# Include directories
target_include_directories(${LIBNAME} PRIVATE ${PROJECT_SOURCE_DIR})
target_include_directories(${LIBNAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
                                             $<INSTALL_INTERFACE:include/${SRCNAME}>
                                             $<INSTALL_INTERFACE:include>)

# Link dependencies
target_link_libraries(milkprocessinfo PUBLIC ImageStreamIO rt pthread m)

install(TARGETS milkprocessinfo EXPORT milkTargets DESTINATION lib)

# Add milk-procinfo-help executable
install(FILES
    pinfo_struct.h
    pinfo_struct_api.h
    pinfo_exec.h
    DESTINATION include)
