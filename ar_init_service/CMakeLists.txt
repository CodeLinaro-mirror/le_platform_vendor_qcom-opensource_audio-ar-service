cmake_minimum_required(VERSION 3.5)

project(ar_init_service)

set(ar_init_service_includes
    inc/)

include_directories(${ar_init_service_includes})

set(ar_init_service_SOURCE
    src/ar_init_service.c
)

# Build as executable instead of shared library
add_executable(ar_init_service ${ar_init_service_SOURCE})

# Link required libraries
target_link_libraries(ar_init_service PRIVATE audio_log_util agmmixer agmserverwrapper tinyalsa mmosal agm ar-gsl ar-osal)

# Install binary and headers
install(TARGETS ar_init_service RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY ${ar_init_service_includes} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")