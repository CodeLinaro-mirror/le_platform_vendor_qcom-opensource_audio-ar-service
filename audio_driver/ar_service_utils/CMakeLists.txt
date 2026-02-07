cmake_minimum_required(VERSION 3.5)

project(ar_service_utils)

set(ar_service_utils_includes
    inc/)

include_directories(${ar_service_utils_includes})

set(ar_service_utils_SOURCE
    src/ar_prm.c)

add_library(ar_service_utils SHARED ${ar_service_utils_SOURCE})
target_link_libraries(ar_service_utils PRIVATE agmserverwrapper tinyalsa mmosal agm ar-gsl ar-osal)

install(TARGETS ar_service_utils LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY ${ar_service_utils_includes} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")