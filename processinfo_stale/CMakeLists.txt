cmake_minimum_required(VERSION 3.17)

set(LIBNAME "milkprocessinfo")

project(${LIBNAME} LANGUAGES C)

set(SOURCES
    processinfo_exec.c
    processinfo_compute_status.c
    processinfo_globals.c
    processinfo_loopstep.c
    processinfo_procdirname.c
    processinfo_setup.c
    processinfo_shm_close.c
    processinfo_shm_create.c
    processinfo_shm_link.c
    processinfo_shm_list_create.c
    processinfo_SIGexit.c
    processinfo_signals.c
    processinfo_update_output_stream.c
    processinfo_WriteMessage.c
    processtools_trigger.c
    quicksort.c
    timeutils.c
)

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
    processinfo.h
    processtools.h
    processtools_trigger.h
    processinfo_exec_end.h
    processinfo_exec_start.h
    processinfo_loopstep.h
    processinfo_procdirname.h
    processinfo_setup.h
    processinfo_shm_close.h
    processinfo_shm_create.h
    processinfo_shm_link.h
    processinfo_shm_list_create.h
    processinfo_SIGexit.h
    processinfo_signals.h
    processinfo_update_output_stream.h
    processinfo_WriteMessage.h
    quicksort.h
    timeutils.h
    DESTINATION include)
