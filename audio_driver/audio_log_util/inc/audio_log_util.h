/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __AUDIO_LOG_UTIL_H__
#define __AUDIO_LOG_UTIL_H__

#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

/* macros that are used to define log severity */
#define BASE_LOG_CRITICAL         (0x1) /**< critical message, log unrecoverable condition .*/
#define BASE_LOG_ERROR            (0x2) /**< error message, represents code bugs that should be debugged and fixed.*/
#define BASE_LOG_INFO             (0x4) /**< info message, provides use information such as state of the program.*/
#define BASE_LOG_DEBUG            (0x8) /**< debug message, required for developers to debug a problem */
#define BASE_LOG_VERBOSE          (0x10)/**< verbose message, useful primarily to help developers debug low-level code */

/* enum that are used to define module IDs */
typedef enum {
  MODULE_AUDIO_SERVICE   = 0x1,
  MODULE_AMFS            = 0x2,
  MODULE_ASMMU           = 0x4,
  MODULE_VIO_AUD_DEV     = 0x8,
  MODULE_AR_DEV_PLUGIN   = 0x10,
  MODULE_AR_SCMI         = 0x20,
  /* Legacy aggregate policy entries retained for config compatibility. */
  MODULE_AGM             = 0x10000,
  MODULE_ARGS            = 0x20000,
  MODULE_AGM_CORE        = 0x40000,
  MODULE_AGM_IPC_CLIENT  = 0x80000,
  MODULE_AGM_IPC_SERVER  = 0x100000,
  MODULE_AGM_TINYALSA_PLUGIN = 0x200000,
  MODULE_AGM_ALSALIB_PLUGIN  = 0x400000,
  MODULE_ARGS_OSAL       = 0x800000,
  MODULE_ARGS_UTIL       = 0x1000000,
  MODULE_ARGS_GPR        = 0x2000000,
  MODULE_ARGS_GSL        = 0x4000000,
  MODULE_ARGS_ACDB       = 0x8000000,
  MODULE_ARGS_ATS        = 0x10000000,
  MODULE_ARGS_SPF        = 0x20000000,
  //default module id
  MODULE_AUDIO           = 0x80000000
} module_id_t;

static inline const char *audio_log_file_short(const char *file)
{
    const char *p = strrchr(file, '/');

    return p ? p + 1 : file;
}

#define AUDIO_BASE_LOG(level, module_id, fmt, ...)         \
do{                                                        \
    (void)audio_log_logger(level, module_id,               \
    ":[%s:%d]: %s: " fmt,                                  \
    audio_log_file_short(__FILE__),                        \
    __LINE__, __func__, ##__VA_ARGS__);                    \
                                                           \
}while(0)                                                  \

/**
  @brief:  audio_log_logger - Convert variadic variables into a buffer before
  configuring the log level

  @param: level      - Comply with macros from audio_log_util.h
  @param: module_id  - Comply with macros from audio_log_util.h
  @param: format     - printf styled string

  @return: 0 - success
           failure otherwise
 */
int audio_log_logger(uint32_t level, uint32_t module_id, const char* format, ...);

/* Load runtime configuration from a file. */
int audio_log_load_config(const char *filename);

/* Runtime module-level control and inspection. */
int audio_log_set_module_level(module_id_t module_id, uint32_t level);
int audio_log_get_module_level(module_id_t module_id, uint32_t *level);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif // __AUDIO_LOG_UTIL_H__
