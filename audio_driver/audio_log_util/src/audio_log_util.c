/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "audio_log_util.h"
#include <errno.h>
#include <stdarg.h>
#include <syslog.h>

/* Audio Log Module Macros */
#define MAX_LOG_BUF_SIZE                         256
#define MAX_TAG_SIZE                              20

/* Audio Log Module Specific Error Code */
#define  AUDIO_LOG_INVALID_MODULE_ID            -100
#define  AUDIO_LOG_INVALID_BUFFER_SIZE          -101
#define  AUDIO_LOG_INVALID_LOG_LEVEL            -102

/* Min log level used by the modules*/
#define MIN_LOG_LEVEL (BASE_LOG_ERROR|BASE_LOG_INFO)
/* Max log level used by the modules*/
#define MAX_LOG_LEVEL (BASE_LOG_CRITICAL|BASE_LOG_ERROR|BASE_LOG_INFO|BASE_LOG_DEBUG|BASE_LOG_VERBOSE)

static void logger_log(const char* tag, int severity, const char* fmt, ... )
{
    va_list ap;
    char buf[MAX_LOG_BUF_SIZE];
    va_start(ap, fmt);
    vsnprintf(buf, MAX_LOG_BUF_SIZE, fmt, ap);
    va_end(ap);
    syslog(severity, "%s - %s", tag, buf);
    //__android_log_write(severity, tag, buf);
}

/* Variable that is used to control log module(s) */
uint32_t audio_base_modules = (MODULE_AUDIO|MODULE_AUDIO_SERVICE|MODULE_AMFS|MODULE_CSD2|MODULE_MCM|MODULE_ASMMU|MODULE_CSD2_IPC
                               |MODULE_AVMM|MODULE_AWLOOP|MODULE_AUDIO_CHIME|MODULE_CSD2_OEM_PLUGIN|MODULE_VIO_AUD_DEV
                               |MODULE_AMS2|MODULE_AMS2_TEST|MODULE_AUDIO_OEM_CONF|MODULE_AR_DEV_PLUGIN|MODULE_AR_SCMI);

struct audio_log_util
{
  module_id_t module_id;
  uint32_t log_lvl;
  char     module_tag[MAX_TAG_SIZE];
};

static int get_module_log_lvl(uint32_t module_id, uint32_t *module_log_lvl);
static int audio_log(uint32_t level, const char* log_tag, char *buf, int32_t bufSz);
static char *get_module_tag(uint32_t module_id);

/* map that is used to mapping module tag name, and log level based on module ID.
   If module id is MODULE_AUDIO, a default log level will be used */
static struct audio_log_util audio_log_util_map[] = {
    {.module_id = MODULE_AUDIO, .log_lvl = MIN_LOG_LEVEL, .module_tag = "AUDIO"},
    {.module_id = MODULE_AUDIO_SERVICE, .log_lvl = MIN_LOG_LEVEL, .module_tag = "AUDIO-SERVICE"},
    {.module_id = MODULE_CSD2, .log_lvl = MIN_LOG_LEVEL, .module_tag = "CSD2"},
    {.module_id = MODULE_AMFS, .log_lvl = MIN_LOG_LEVEL, .module_tag = "AMFS"},
    {.module_id = MODULE_MCM, .log_lvl = MIN_LOG_LEVEL, .module_tag = "MCM"},
    {.module_id = MODULE_ASMMU, .log_lvl = MIN_LOG_LEVEL, .module_tag = "ASMMU"},
    {.module_id = MODULE_CSD2_IPC, .log_lvl = MIN_LOG_LEVEL, .module_tag = "CSD2-IPC"},
    {.module_id = MODULE_AVMM, .log_lvl = MIN_LOG_LEVEL, .module_tag = "AVMM"},
    {.module_id = MODULE_AWLOOP, .log_lvl = MIN_LOG_LEVEL, .module_tag = "AWLOOP"},
    {.module_id = MODULE_AUDIO_CHIME, .log_lvl = MIN_LOG_LEVEL, .module_tag = "AUDIO-CHIME"},
    {.module_id = MODULE_CSD2_OEM_PLUGIN, .log_lvl = MIN_LOG_LEVEL, .module_tag = "CSD2-OEM-PLUGIN"},
    {.module_id = MODULE_VIO_AUD_DEV, .log_lvl = MIN_LOG_LEVEL, .module_tag = "VIO-AUD-DEV"},
    {.module_id = MODULE_AMS2, .log_lvl = MIN_LOG_LEVEL, .module_tag = "AMS2"},
    {.module_id = MODULE_AMS2_TEST, .log_lvl = MIN_LOG_LEVEL, .module_tag = "AMS2-TEST"},
    {.module_id = MODULE_AUDIO_OEM_CONF, .log_lvl = MIN_LOG_LEVEL, .module_tag = "AUDIO-OEM-CONF"},
    {.module_id = MODULE_AR_DEV_PLUGIN, .log_lvl = MIN_LOG_LEVEL, .module_tag = "AR-DEV-PLUGIN"},
    {.module_id = MODULE_AR_SCMI, .log_lvl = MIN_LOG_LEVEL, .module_tag = "AR-SCMI"},
};

static int get_module_log_lvl(uint32_t module_id, uint32_t *module_log_lvl)
{
  uint8_t i = 0;
  uint32_t cnt = (uint32_t)sizeof(audio_log_util_map) / (uint32_t)sizeof(audio_log_util_map[0]);

  if (module_log_lvl == NULL)
  {
    logger_log("AUDIO-LOG-UTIL", LOG_ERROR, "Invalid args");
    return -EINVAL;
  }

  for (i=0;i<cnt;++i)
  {
    if (audio_log_util_map[i].module_id == module_id)
    {
      *module_log_lvl = audio_log_util_map[i].log_lvl;
      return 0;
    }
  }

  logger_log("AUDIO-LOG-UTIL", LOG_ERROR, "Module id is not found");

  return -EINVAL;
}

static int audio_log(uint32_t level, const char* log_tag, char *buf, int32_t bufSz)
{
    if (buf == NULL || bufSz > MAX_LOG_BUF_SIZE)
    {
        logger_log("AUDIO-LOG-UTIL", LOG_ERROR, "audio_log Invalid bufSz or buff");
        return -EINVAL;
    }

    switch (level)
    {
      case BASE_LOG_CRITICAL:
        logger_log(log_tag, LOG_FATAL, "%s", buf);
        break;

      case BASE_LOG_ERROR:
        logger_log(log_tag, LOG_ERROR, "%s", buf);
        break;

      case BASE_LOG_DEBUG:
        logger_log(log_tag, LOG_DEBUG, "%s", buf);
        break;

      case BASE_LOG_INFO:
        logger_log(log_tag, LOG_INFO, "%s", buf);
        break;

      case BASE_LOG_VERBOSE:
        logger_log(log_tag, LOG_VERBOSE, "%s", buf);
        break;

      default:
        logger_log("AUDIO-LOG-UTIL", LOG_UNKNOWN, "Invalid log level");
        return -EINVAL;
    }

    return 0;
}

static char *get_module_tag(uint32_t module_id)
{
  uint8_t i = 0;
  uint32_t cnt = (uint32_t)sizeof(audio_log_util_map) / (uint32_t)sizeof(audio_log_util_map[0]);

  for (i=0;i<cnt;++i)
  {
    if (audio_log_util_map[i].module_id == module_id)
      return audio_log_util_map[i].module_tag;
  }

  logger_log("AUDIO-LOG-UTIL", LOG_ERROR, "Failed to get module tag");

  return "NONE";
}

int audio_log_logger(uint32_t level, uint32_t module_id, const char* format, ...)
{
    va_list arglist = {0};
    int32_t size;
    uint32_t module_log_lvl = 0;
    uint32_t rc = 0;

    char buf[MAX_LOG_BUF_SIZE] = {0};
    char buf_tmp[MAX_LOG_BUF_SIZE] = {0};

    // check if a non-zero signed module id is valid and enabled
    if ((module_id & audio_base_modules) == 0)
    {
      logger_log("AUDIO-LOG-UTIL", LOG_ERROR, "Invalid module Id %u", module_id);
      return AUDIO_LOG_INVALID_MODULE_ID;
    }

    // fetch the log level based on module id
    rc = get_module_log_lvl(module_id, &module_log_lvl);
    if (rc)
    {
      logger_log("AUDIO-LOG-UTIL", LOG_ERROR, "getModuleLogLvl() failed: %u", rc);
      return AUDIO_LOG_INVALID_MODULE_ID;
    }

    // fetch the log tag based on module id
    char* log_tag = get_module_tag(module_id);

    va_start(arglist, format);
    size = vsnprintf(buf, MAX_LOG_BUF_SIZE, format, arglist);
    va_end(arglist);

    if (size < 0 || size > MAX_LOG_BUF_SIZE)
    {
      logger_log("AUDIO-LOG-UTIL", LOG_ERROR, "log buf size is less than zero: %d", size);
      return AUDIO_LOG_INVALID_BUFFER_SIZE;
    }

    // determine the log level severity
    if ((level & module_log_lvl) == 0)
    {
      return AUDIO_LOG_INVALID_LOG_LEVEL;
    }

    // generate the log
    rc = audio_log(level, log_tag, buf, size);
    if (rc)
    {
      logger_log("AUDIO-LOG-UTIL", LOG_ERROR, "audio_log_logger() failed : %u", rc);
    }

    return rc;
}
