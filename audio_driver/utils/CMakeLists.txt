cmake_minimum_required(VERSION 3.5)

project(audio_utils)

set(audio_utils_includes
    inc/)

include_directories(${audio_utils_includes})

set(audio_utils_SOURCE
    src/audio_smmu_helper.c)

if("${ENABLE_TARGET}" STREQUAL "sa8775")
    add_definitions(-DENABLE_AMS_SMMU)
elseif("${ENABLE_TARGET}" STREQUAL "sa7255")
    add_definitions(-DPLATFORM_MONACO)
    add_definitions(-DENABLE_AMS_SMMU)
elseif(("${ENABLE_TARGET}" STREQUAL "sa8295") OR ("${ENABLE_TARGET}" STREQUAL "sa8540"))
    ##add_definitions(-DENABLE_AMS_SMMU)
endif()

add_definitions(-DVFIO_DEV_SMMU_DSP0="1.vfio_audio_adsp0")
add_definitions(-Dstrlcpy=g_strlcpy)
add_definitions(-Dstrlcat=g_strlcat)

add_library(audio_utils SHARED ${audio_utils_SOURCE})
target_link_libraries(audio_utils PUBLIC kiumd audio_log_util)

install(TARGETS audio_utils LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY ${audio_utils_includes} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
