/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __AUDIO_LOG_UTIL_H__
#define __AUDIO_LOG_UTIL_H__

#include <stdio.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

/* macros that are used to define log severity */
#define BASE_LOG_CRITICAL         (0x1) /**< critical message, log unrecoverable condition .*/
#define BASE_LOG_ERROR            (0x2) /**< error message, represents code bugs that should be debugged and fixed.*/
#define BASE_LOG_INFO             (0x4) /**< info message, provides use information such as state of the program.*/
#define BASE_LOG_DEBUG            (0x8) /**< debug message, required for developers to debug a problem */
#define BASE_LOG_VERBOSE          (0x10)/**< verbose message, useful primarily to help developers debug low-level code */

typedef enum log_prio {
    LOG_UNKNOWN = 0,
    LOG_DEFAULT,
    LOG_VERBOSE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
} log_prio;

/* enum that are used to define module IDs */
typedef enum {
  MODULE_AUDIO_SERVICE   = 0x1,
  MODULE_CSD2            = 0x2,
  MODULE_AMFS            = 0x4,
  MODULE_MCM             = 0x8,
  MODULE_ASMMU           = 0x10,
  MODULE_CSD2_IPC        = 0x20,
  MODULE_AVMM            = 0x40,
  MODULE_AWLOOP          = 0x80,
  MODULE_AUDIO_CHIME     = 0x100,
  MODULE_CSD2_OEM_PLUGIN = 0x200,
  MODULE_VIO_AUD_DEV     = 0x400,
  MODULE_AMS2            = 0x800,
  MODULE_AMS2_TEST       = 0x1000,
  MODULE_AUDIO_OEM_CONF  = 0x2000,
  MODULE_AR_DEV_PLUGIN   = 0x4000,
  MODULE_AR_SCMI         = 0x8000,
  //default module id
  MODULE_AUDIO           = 0x80000000
} module_id_t;

#define __FILENAME__ (strrchr(__FILE__, '/') ? \
        strrchr(__FILE__, '/') + 1 : __FILE__)

#define AUDIO_BASE_LOG(level, module_id, fmt, ...)         \
do{                                                        \
    (void)audio_log_logger(level, module_id,               \
    ":[%s:%d]: %s: " fmt,                                  \
    __FILENAME__,                                          \
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

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif // __AUDIO_LOG_UTIL_H__
