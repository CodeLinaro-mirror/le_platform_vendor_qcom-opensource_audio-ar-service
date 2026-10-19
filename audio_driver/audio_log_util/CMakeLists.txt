cmake_minimum_required(VERSION 3.5)

project(audio_log_util)

set(audio_log_util_includes
    inc/)

include_directories(${audio_log_util_includes})

set(audio_log_util_SOURCE
    src/audio_log_util.c)

add_library(audio_log_util SHARED ${audio_log_util_SOURCE})
target_link_libraries(audio_log_util PUBLIC glib-2.0)
set_target_properties(audio_log_util PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

install(TARGETS audio_log_util LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY ${audio_log_util_includes} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
