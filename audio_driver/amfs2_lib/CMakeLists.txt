cmake_minimum_required(VERSION 3.5)

project(amfs2)

set(amfs2_includes
    common/inc/
    inc/)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

include_directories(${amfs2_includes})
include_directories(${GLIB_INCLUDE_DIRS})

set(amfs2_SOURCE
    be_lib/src/amfs_core.c
    be_lib/src/amfs_impl_remote.c
    common/src/amfs_impl_std.c)

add_definitions(-DUSE_GLIB)
add_definitions(-Dstrlcpy=g_strlcpy)
add_definitions(-Dstrlcat=g_strlcat)

add_library(amfs2 SHARED ${amfs2_SOURCE})
target_link_libraries (amfs2 libstd glib-2.0)

install(TARGETS amfs2 LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY ${amfs2_includes} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
