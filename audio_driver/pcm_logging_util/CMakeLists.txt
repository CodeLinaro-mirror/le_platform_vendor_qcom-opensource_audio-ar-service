cmake_minimum_required(VERSION 3.5)

project(pcm_logging_util)

set(pcm_logging_util_includes
    inc/)

include_directories(${pcm_logging_util_includes})

set(pcm_logging_util_source
    src/pcm_logging_util.c)

add_executable(pcm_logging_util ${pcm_logging_util_source})

target_link_libraries(pcm_logging_util PRIVATE agmclient)
target_link_libraries(pcm_logging_util PRIVATE pthread dl)

install(TARGETS pcm_logging_util RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY ${pcm_logging_util_includes} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
