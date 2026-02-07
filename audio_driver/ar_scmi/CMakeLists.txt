cmake_minimum_required(VERSION 3.5)

project(ar_scmi)

set(ar_scmi_includes
    inc/)

include_directories(${ar_scmi_includes})

set(ar_scmi_SOURCE
    src/ar_scmi.c)

add_definitions(-Dstrlcpy=g_strlcpy)

add_library(ar_scmi SHARED ${ar_scmi_SOURCE})
target_link_libraries(ar_scmi PUBLIC audio_utils)

install(TARGETS ar_scmi LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY ${ar_scmi_includes} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
