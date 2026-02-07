cmake_minimum_required(VERSION 3.5)

project(ar_dev_plugin)

set(ar_dev_plugin_includes)

include_directories(${ar_dev_plugin_includes}
)

add_definitions(-DI2C_AUDIO_DAC_ADDR=0x4C)
if("${ENABLE_TARGET}" STREQUAL "sa8797")
    add_definitions(-DI2C_EXPANDERS_ADDR=0x20)
    add_definitions(-DI2C_DEVICE=18)
    add_definitions(-DI2C_AUDIO_ADC_ADDR=0x31)
else()
    add_definitions(-DI2C_EXPANDERS_ADDR=0x77)
    add_definitions(-DI2C_DEVICE=11)
    add_definitions(-DMUX_GPIO=51)
endif()

if("${ENABLE_TARGET}" STREQUAL "sa8797")
    add_library(ar_dev_plugin SHARED ar_dev_plugin_v2.c)
    target_link_libraries(ar_dev_plugin PUBLIC ar_scmi audio_log_util audio_expander audio_dac audio_adc)
else()
    add_library(ar_dev_plugin SHARED ar_dev_plugin.c)
    target_link_libraries(ar_dev_plugin PUBLIC audio_log_util audio_expander audio_dac audio-nxp-auto)
endif()

install(TARGETS ar_dev_plugin LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY ${ar_dev_plugin_includes} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
