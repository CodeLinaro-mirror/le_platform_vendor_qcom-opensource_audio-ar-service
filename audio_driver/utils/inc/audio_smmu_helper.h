#ifndef __AUDIO_SMMU_HELPER_H__
#define __AUDIO_SMMU_HELPER_H__

/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "mmdefs.h"

#include "plat_vfio.h"
#include "plat_smmu.h"
#include "audio_log_util.h"

#define DEFAULT_ADSP_SID 1
#define DEFAULT_GPDSP0_SID 2
#define DEFAULT_GPDSP1_SID 2

#define DEVICE_NAME_ADSP "1000.audio"
#define DEVICE_NAME_GPDSP0 "2.vfio_audio_gpdsp0_cb"
#define DEVICE_NAME_GPDSP1 "2.vfio_audio_gpdsp1_cb"

#ifdef ENABLE_AMS_SMMU
  #ifdef PLATFORM_MONACO
#define DEVICE_NUM_MAX 2
  #else
#define DEVICE_NUM_MAX 3
  #endif
#else
#define DEVICE_NUM_MAX 1
#endif

/* Naming convention of dev_name is
 * <sid>.vfio_audio_<dsp>_cb
 */
static char* dev_name[DEVICE_NUM_MAX] =
{
    "1.vfio_audio_adsp0",
#ifdef ENABLE_AMS_SMMU
    "2.vfio_audio_gpdsp0_cb",
  #ifndef PLATFORM_MONACO
    "2.vfio_audio_gpdsp1_cb",
  #endif
#endif
};

enum dsp_type {
  ADSP = 0,
  GPDSP0,
  GPDSP1,
  DSP_TYPE_MAX,
};

#ifdef PLATFORM_MONACO
#define SMMU_DSP_TYPE GPDSP0 //GPDSP0 is used for AMS usecase on Monaco
#else
#define SMMU_DSP_TYPE GPDSP1
#endif

typedef struct audio_smmu_helper
{
  uint8_t sid;
  uint32_t dsp_type;
  struct plat_smmu smmu_ctx;
}audio_smmu_helper_t;

/**
  Creates a new instance of audio SMMU helper.

  @param[in]  config      Pointer to audio_smmu_helper_config structure
                          containing configuration data for the new helper
                          instance.
  @return                 Pointer to the new audio SMMU helper instance, or
                          NULL in case of error.
*/
audio_smmu_helper_t* audio_smmu_helper_create();

/**
  Create a new address mapping.

  @param[in]  h           Pointer to audio_smmu_helper instance.
  @return                 Mapped address (device address), or NULL in case of
                          error.
*/
void* audio_smmu_helper_map(audio_smmu_helper_t* h);

/**
  Remove an address mapping.

  @param[in]  h           Pointer to audio_smmu_helper instance.
  @param[in]  dev_addr    Device address to be unmapped.
  @param[in]  size        Size of the memory region to be unmapped (bytes).
  @return                 0 in case of success, or NULL in case of error.
*/
int audio_smmu_helper_unmap(audio_smmu_helper_t* h);

/**
  Destroy an instance of audio SMMU helper.

  @param[in]  h           Pointer to audio_smmu_helper instance.
*/
void audio_smmu_helper_destroy(audio_smmu_helper_t* h);

/**
  Initialize the audio SMMU.  Should be called only once at system startup,
  before the APIs above are called for mapping/unmapping.
*/
int audio_smmu_helper_init();

/**
  Deinitialize the audio SMMU.  Should be called only when service exits.
*/
int audio_smmu_helper_deinit();

#endif /* __AUDIO_SMMU_HELPER_H__ */

