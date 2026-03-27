/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef AMFS_DEBUG_H
#define AMFS_DEBUG_H

#include <stdio.h>

#include "audio_log_util.h"

#define AMFS_FREEIF(pv) \
  do {\
    if (pv) {\
      void *tmp = (void *)pv;\
      pv = NULL;\
      free(tmp);\
    } \
  } while(0)


#ifndef AMFS_VERIFY
  #define AMFS_VERIFY(val, err, bail, ...) \
     do {\
      if(0 == (val)) {\
       (err) = ((err) == 0 ? -1 : (err));\
       AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, #val" - Verify failed");\
       AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, __VA_ARGS__);\
       goto bail;\
      } else {\
       AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AMFS, #val" - Verify passed");\
      }\
     } while(0)
#endif /* AMFS_VERIFY */
#endif /* AMFS_DEBUG_H */
