cmake_minimum_required(VERSION 3.5)

project(audio_service)

find_package(PkgConfig)
pkg_check_modules (GLIB2 glib-2.0>=2.10)
find_package(PkgConfig REQUIRED)
    pkg_check_modules(DBUS dbus-1>=1.4.12)
include_directories(${GLIB2_INCLUDE_DIRS} ${DBUS_INCLUDE_DIRS})
link_directories(${GLIB2_LIBRARIES} ${DBUS_LIBRARIES})
set(LIBS ${LIBS} ${GLIB2_LIBRARIES} ${DBUS_LIBRARIES})

set(audio_service_includes
    inc/)

include_directories(${audio_service_includes})

set(audio_service_SOURCE
    src/audio_stubs.c
    src/coreinit.c
    src/main.c)

if("${ENABLE_TARGET}" STREQUAL "sa8775" OR "${ENABLE_TARGET}" STREQUAL "sa8797")
    add_definitions(-DENABLE_MDF)
endif()

add_executable(audio_service ${audio_service_SOURCE})

target_link_libraries(audio_service PRIVATE pthread dl glib-2.0 rt)
target_link_libraries(audio_service PRIVATE audio_log_util ${LIBS})
target_link_libraries(audio_service PRIVATE agmserverwrapper ar_service_utils)
target_link_libraries(audio_service PRIVATE AudioDeviceManager)
target_link_libraries(audio_service PRIVATE glink_client pil-client ar_dev_plugin)
target_link_libraries(audio_service PRIVATE systemd)

## Needed when below libs are statically linked
target_link_libraries(audio_service PRIVATE ar-gsl)

install(TARGETS audio_service RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY ${audio_service_includes} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
