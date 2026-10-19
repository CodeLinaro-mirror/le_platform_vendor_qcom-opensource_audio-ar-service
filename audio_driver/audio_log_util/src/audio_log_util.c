/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "audio_log_util.h"
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

/* Audio Log Module Macros */
#define MAX_LOG_BUF_SIZE                         256
#define MAX_TAG_SIZE                              20
#define MAX_CONFIG_ENTRIES                       32
#define AUDIO_LOG_CONFIG_PATH                    "/etc/ar_log_config"

/* Audio Log Module Specific Error Code */
#define  AUDIO_LOG_INVALID_MODULE_ID            -100
#define  AUDIO_LOG_INVALID_BUFFER_SIZE          -101
#define  AUDIO_LOG_INVALID_LOG_LEVEL            -102

/* Min log level used by the modules*/
#define MIN_LOG_LEVEL (BASE_LOG_ERROR|BASE_LOG_INFO)
/* Max log level used by the modules*/
#define MAX_LOG_LEVEL (BASE_LOG_CRITICAL|BASE_LOG_ERROR|BASE_LOG_INFO|BASE_LOG_DEBUG|BASE_LOG_VERBOSE)
#define ALL_KNOWN_MODULES (MODULE_AUDIO_SERVICE|MODULE_AMFS|MODULE_ASMMU \
                          |MODULE_VIO_AUD_DEV|MODULE_AR_DEV_PLUGIN|MODULE_AR_SCMI \
                          |MODULE_AGM|MODULE_ARGS|MODULE_AGM_CORE \
                          |MODULE_AGM_IPC_CLIENT|MODULE_AGM_IPC_SERVER \
                          |MODULE_AGM_TINYALSA_PLUGIN|MODULE_AGM_ALSALIB_PLUGIN \
                          |MODULE_ARGS_OSAL|MODULE_ARGS_UTIL|MODULE_ARGS_GPR \
                          |MODULE_ARGS_GSL|MODULE_ARGS_ACDB|MODULE_ARGS_ATS \
                          |MODULE_ARGS_SPF|MODULE_AUDIO)

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
static atomic_uint audio_base_modules = ATOMIC_VAR_INIT(ALL_KNOWN_MODULES);

struct audio_log_util
{
  module_id_t module_id;
  atomic_uint log_lvl;
  char     module_tag[MAX_TAG_SIZE];
};

static int get_module_log_lvl(uint32_t module_id, uint32_t *module_log_lvl);
static int audio_log(uint32_t level, const char* log_tag, char *buf, int32_t bufSz);
static char *get_module_tag(uint32_t module_id);

struct audio_log_config_entry
{
  module_id_t module_id;
  uint32_t level;
};

/* map that is used to mapping module tag name, and log level based on module ID.
   If module id is MODULE_AUDIO, a default log level will be used */
static struct audio_log_util audio_log_util_map[] = {
    {.module_id = MODULE_AUDIO, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "AUDIO"},
    {.module_id = MODULE_AUDIO_SERVICE, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "AUDIO-SERVICE"},
    {.module_id = MODULE_AMFS, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "AMFS"},
    {.module_id = MODULE_ASMMU, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "ASMMU"},
    {.module_id = MODULE_VIO_AUD_DEV, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "VIO-AUD-DEV"},
    {.module_id = MODULE_AR_DEV_PLUGIN, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "AR-DEV-PLUGIN"},
    {.module_id = MODULE_AR_SCMI, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "AR-SCMI"},
    {.module_id = MODULE_AGM, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "AGM"},
    {.module_id = MODULE_ARGS, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "ARGS"},
    {.module_id = MODULE_AGM_CORE, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "AGM-CORE"},
    {.module_id = MODULE_AGM_IPC_CLIENT, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "AGM-IPC-CLIENT"},
    {.module_id = MODULE_AGM_IPC_SERVER, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "AGM-IPC-SERVER"},
    {.module_id = MODULE_AGM_TINYALSA_PLUGIN, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "AGM-TINYALSA-PLUGIN"},
    {.module_id = MODULE_AGM_ALSALIB_PLUGIN, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "AGM-ALSALIB-PLUGIN"},
    {.module_id = MODULE_ARGS_OSAL, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "ARGS-OSAL"},
    {.module_id = MODULE_ARGS_UTIL, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "ARGS-UTIL"},
    {.module_id = MODULE_ARGS_GPR, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "ARGS-GPR"},
    {.module_id = MODULE_ARGS_GSL, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "ARGS-GSL"},
    {.module_id = MODULE_ARGS_ACDB, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "ARGS-ACDB"},
    {.module_id = MODULE_ARGS_ATS, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "ARGS-ATS"},
    {.module_id = MODULE_ARGS_SPF, .log_lvl = ATOMIC_VAR_INIT(MIN_LOG_LEVEL), .module_tag = "ARGS-SPF"},
};

static int get_module_log_lvl(uint32_t module_id, uint32_t *module_log_lvl)
{
  uint8_t i = 0;
  uint32_t cnt = (uint32_t)sizeof(audio_log_util_map) / (uint32_t)sizeof(audio_log_util_map[0]);

  if (module_log_lvl == NULL)
  {
    logger_log("AUDIO-LOG-UTIL", LOG_ERR, "Invalid args");
    return -EINVAL;
  }

  for (i=0;i<cnt;++i)
  {
    if (audio_log_util_map[i].module_id == module_id)
    {
      *module_log_lvl = atomic_load_explicit(&audio_log_util_map[i].log_lvl,
                                              memory_order_acquire);
      return 0;
    }
  }

  logger_log("AUDIO-LOG-UTIL", LOG_ERR, "Module id is not found");

  return -EINVAL;
}

static int audio_log(uint32_t level, const char* log_tag, char *buf, int32_t bufSz)
{
    if (buf == NULL || bufSz > MAX_LOG_BUF_SIZE)
    {
        logger_log("AUDIO-LOG-UTIL", LOG_ERR, "audio_log Invalid bufSz or buff");
        return -EINVAL;
    }

    switch (level)
    {
      case BASE_LOG_CRITICAL:
        logger_log(log_tag, LOG_CRIT, "%s", buf);
        break;

      case BASE_LOG_ERROR:
        logger_log(log_tag, LOG_ERR, "%s", buf);
        break;

      case BASE_LOG_DEBUG:
        logger_log(log_tag, LOG_DEBUG, "%s", buf);
        break;

      case BASE_LOG_INFO:
        logger_log(log_tag, LOG_INFO, "%s", buf);
        break;

      case BASE_LOG_VERBOSE:
        logger_log(log_tag, LOG_DEBUG, "%s", buf);
        break;

      default:
        logger_log("AUDIO-LOG-UTIL", LOG_ERR, "Invalid log level");
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

  logger_log("AUDIO-LOG-UTIL", LOG_ERR, "Failed to get module tag");

  return "NONE";
}

int audio_log_get_module_level(module_id_t module_id, uint32_t *level)
{
    if (level == NULL)
        return -EINVAL;

    return get_module_log_lvl(module_id, level);
}

int audio_log_set_module_level(module_id_t module_id, uint32_t level)
{
    uint32_t cnt = (uint32_t)(sizeof(audio_log_util_map) /
                              sizeof(audio_log_util_map[0]));

    if ((level & ~MAX_LOG_LEVEL) != 0)
        return -EINVAL;

    for (uint32_t i = 0; i < cnt; ++i) {
        if (audio_log_util_map[i].module_id == module_id) {
            atomic_store_explicit(&audio_log_util_map[i].log_lvl, level,
                                  memory_order_release);
            return 0;
        }
    }

    return -EINVAL;
}

static int audio_log_find_module(const char *tag, module_id_t *module_id)
{
    uint32_t cnt = (uint32_t)(sizeof(audio_log_util_map) /
                              sizeof(audio_log_util_map[0]));

    if (tag == NULL || module_id == NULL)
        return -EINVAL;

    for (uint32_t i = 0; i < cnt; ++i) {
        if (strcmp(tag, audio_log_util_map[i].module_tag) == 0) {
            *module_id = audio_log_util_map[i].module_id;
            return 0;
        }
    }

    return -EINVAL;
}

int audio_log_load_config(const char *filename)
{
    FILE *file;
    char line[MAX_LOG_BUF_SIZE];
    struct audio_log_config_entry entries[MAX_CONFIG_ENTRIES];
    uint32_t entry_count = 0;
    uint32_t module_mask = atomic_load_explicit(&audio_base_modules,
                                                memory_order_acquire);
    int module_mask_changed = 0;
    int invalid = 0;

    if (filename == NULL)
        return -EINVAL;

    file = fopen(filename, "r");
    if (file == NULL)
        return (errno == ENOENT) ? -ENOENT : -errno;

    while (fgets(line, sizeof(line), file) != NULL) {
        char *cursor = line;
        char event[32] = {0};
        char tag[MAX_TAG_SIZE] = {0};
        unsigned int value = 0;
        unsigned int version = 0;
        module_id_t module_id = 0;

        while (isspace((unsigned char)*cursor))
            ++cursor;
        if (*cursor == '\0' || *cursor == '#')
            continue;

        if (sscanf(cursor, "%u %31s", &version, event) != 2 ||
            version != 1) {
            invalid = 1;
            continue;
        }

        if (strcmp(event, "enable_log_module") == 0) {
            if (sscanf(cursor, "%*u %*31s %x", &value) != 1 ||
                (value & ~ALL_KNOWN_MODULES) != 0) {
                invalid = 1;
                continue;
            }
            module_mask = value;
            module_mask_changed = 1;
            continue;
        }

        if (strcmp(event, "update_log_lvl") != 0 ||
            sscanf(cursor, "%*u %*31s %19s %x", tag, &value) != 2 ||
            audio_log_find_module(tag, &module_id) != 0 ||
            (value & ~MAX_LOG_LEVEL) != 0 ||
            entry_count >= MAX_CONFIG_ENTRIES) {
            invalid = 1;
            continue;
        }

        entries[entry_count].module_id = module_id;
        entries[entry_count].level = value;
        ++entry_count;
    }

    fclose(file);

    if (invalid || (entry_count == 0 && !module_mask_changed))
        return -EINVAL;

    if (module_mask_changed)
        atomic_store_explicit(&audio_base_modules, module_mask,
                              memory_order_release);
    for (uint32_t i = 0; i < entry_count; ++i)
        (void)audio_log_set_module_level(entries[i].module_id,
                                         entries[i].level);

    return 0;
}

int audio_log_logger(uint32_t level, uint32_t module_id, const char* format, ...)
{
    va_list arglist = {0};
    int32_t size;
    uint32_t module_log_lvl = 0;
    uint32_t rc = 0;

    char buf[MAX_LOG_BUF_SIZE] = {0};
    // check if a non-zero signed module id is valid and enabled
    if ((module_id & atomic_load_explicit(&audio_base_modules,
                                          memory_order_acquire)) == 0)
    {
      logger_log("AUDIO-LOG-UTIL", LOG_ERR, "Invalid module Id %u", module_id);
      return AUDIO_LOG_INVALID_MODULE_ID;
    }

    // fetch the log level based on module id
    rc = get_module_log_lvl(module_id, &module_log_lvl);
    if (rc)
    {
      logger_log("AUDIO-LOG-UTIL", LOG_ERR, "getModuleLogLvl() failed: %u", rc);
      return AUDIO_LOG_INVALID_MODULE_ID;
    }

    // fetch the log tag based on module id
    char* log_tag = get_module_tag(module_id);

    va_start(arglist, format);
    size = vsnprintf(buf, MAX_LOG_BUF_SIZE, format, arglist);
    va_end(arglist);

    if (size < 0 || size >= MAX_LOG_BUF_SIZE)
    {
      logger_log("AUDIO-LOG-UTIL", LOG_ERR, "log buf size is less than zero: %d", size);
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
      logger_log("AUDIO-LOG-UTIL", LOG_ERR, "audio_log_logger() failed : %u", rc);
    }

    return rc;
}

__attribute__((constructor)) static void audio_log_init_config(void)
{
    /* Missing configuration intentionally preserves the production defaults. */
    (void)audio_log_load_config(AUDIO_LOG_CONFIG_PATH);
}
