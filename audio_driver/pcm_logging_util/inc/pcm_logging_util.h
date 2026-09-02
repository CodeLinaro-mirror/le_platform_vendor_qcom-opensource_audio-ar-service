#ifndef __PCM_LOGGING_UTIL_H__
#define __PCM_LOGGING_UTIL_H__

/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <stdint.h>

#include <agm/agm_api.h>

#define PARAM_ID_DATA_LOGGING_SELECT_CHANNELS    0x080014E8
#define PARAM_ID_DATA_LOGGING_SELECT_CHANNELS_V2 0x08001A66
#define PARAM_ID_MODULE_ENABLE                   0x08001026

#define PCM_LOGGING_MAX_KV_PAIRS 7
#define PCM_LOGGING_MAX_CHANNEL_MASK_GROUPS 4
#define PCM_LOGGING_MAX_V2_WORDS 11

#define SELECTIVE_LOGGING_V1 1
#define SELECTIVE_LOGGING_V2 2

struct pcm_logging_kvp {
    uint32_t num_kvs;
    struct agm_key_value kv[PCM_LOGGING_MAX_KV_PAIRS];
};

struct apm_module_param_data_t {
    uint32_t module_instance_id;
    uint32_t param_id;
    uint32_t param_size;
    uint32_t error_code;
};

typedef struct {
    uint32_t mode;
    uint32_t channel_index_mask;
    uint32_t channel_type_mask_lsw;
    uint32_t channel_type_mask_msw;
} data_logging_select_channels_t;

typedef struct {
    uint32_t enable;
} data_logging_module_enable_t;

#endif
