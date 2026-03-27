/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AUDIO_REACH: prm"

#include "mmdefs.h"
#include "stdlib.h"
#include "ar_osal_heap.h"
#include "MMOSAL.h"
#include "string.h"
#include <ar_prm.h>
#include <agm/utils.h>
#include "prm_api.h"
#include "audio_hw_clk_api.h"
#include <errno.h>
#include "audio_log_util.h"

static void ar_mem_free(void *p)
{
    static ar_heap_info heap_info = {
        .align_bytes = AR_HEAP_ALIGN_DEFAULT,
        .pool_type = AR_HEAP_POOL_DEFAULT,
        .heap_id = AR_HEAP_ID_DEFAULT,
        .tag = AR_HEAP_TAG_DEFAULT
    };

    ar_heap_free(p, &heap_info);
}

/** memory allocation helper functions */
static void *ar_mem_zalloc(size_t size)
{
    void *p;

    static ar_heap_info heap_info = {
        .align_bytes = AR_HEAP_ALIGN_DEFAULT,
        .pool_type = AR_HEAP_POOL_DEFAULT,
        .heap_id = AR_HEAP_ID_DEFAULT,
        .tag = AR_HEAP_TAG_DEFAULT
    };

    /* allocate and initialize with 0 */
    p = ar_heap_calloc(size, &heap_info);

    return p;
}

static uint32_t ar_prm_alloc_packet(uint32_t payload_size, void **payload)
{
    struct ar_prm_header *prm_header = NULL;
    void *packet = NULL;

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Enter, ar_prm_alloc_packet\n");

    packet = calloc(1,payload_size);
    if (!packet)
        return -ENOMEM;

    prm_header = (struct ar_prm_header *)packet;
    prm_header->miid = PRM_MODULE_INSTANCE_ID;
    prm_header->param_id = 0;
    prm_header->param_size = payload_size - sizeof(struct ar_prm_header);
    prm_header->error_code = 0;

    *payload = packet;
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Exit, ar_prm_alloc_packet\n");
    return 0;
}

static int32_t ar_prm_clk_enable_v2(struct ar_clk_config *clks)
{
    int32_t rc = 0;
    audio_hw_clk_cfg_v2_t *clk_enable = NULL;
    audio_hw_clk_cfg_req_param_t *clk_req;
    void *payload = NULL;
    size_t payload_size = 0;
    void *buff = NULL;
    uint32_t buff_size = 0;
    int32_t i;

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Enter: ar_prm_clk_enable_v2");
    //csd2_mutex_lock(csd2_prm_ctx.lock);
    payload_size = sizeof(struct ar_prm_header) + sizeof(audio_hw_clk_cfg_req_param_t) + clks->num_clks * sizeof(audio_hw_clk_cfg_v2_t);
    rc = ar_prm_alloc_packet(payload_size, &payload);

    if (rc)
    {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Error when allocating packet of size %d for CLK_v2!", payload_size);
        goto exit;
    }

    ((struct ar_prm_header *)payload)->param_id = PARAM_ID_RSC_AUDIO_HW_CLK_V2;
    clk_req = (audio_hw_clk_cfg_req_param_t *)((uint8_t *)payload + sizeof(struct ar_prm_header));
    clk_req->num_clock_id = clks->num_clks;

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "clk_v2 req:num %d",clk_req->num_clock_id);

    clk_enable = (audio_hw_clk_cfg_v2_t *)++clk_req;

    for (i=0;i<clks->num_clks;i++)
    {
        clk_enable[i].clock_id = clks->entries[i].id;
        clk_enable[i].clock_freq = clks->entries[i].freq;
        clk_enable[i].clock_attri = clks->entries[i].attri;
        clk_enable[i].clock_root = clks->entries[i].root;
        clk_enable[i].mux_index = clks->entries[i].mux_index;
        clk_enable[i].divider_2x = clks->entries[i].divider_2x;
        clk_enable[i].counter_m_value = clks->entries[i].counter_m_value;
        clk_enable[i].counter_n_value = clks->entries[i].counter_n_value;
        clk_enable[i].d_value = clks->entries[i].d_value;
    }

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "clk_v2 req:id %d", clk_enable[0].clock_id);
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "clk_v2 req:freq %d", clk_enable[0].clock_freq);
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "clk_v2 req:attri %d", clk_enable[0].clock_attri);
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "clk_v2 req:root %d", clk_enable[0].clock_root);
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "clk_v2 req:mux_index %d", clk_enable[0].mux_index);
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "clk_v2 req:divider_2x %d", clk_enable[0].divider_2x);
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "clk_v2 req:counter_m_value %d", clk_enable[0].counter_m_value);
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "clk_v2 req:counter_n_value %d", clk_enable[0].counter_n_value);
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "clk_v2 req:d_value %d", clk_enable[0].d_value);

    rc = gsl_request_hw_rsc_custom_config(payload, payload_size, buff, &buff_size);
    if (rc)
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to request clk_v2!");
exit:
    if (payload)
        ar_mem_free(payload);
   // csd2_mutex_unlock(csd2_prm_ctx.lock);
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Exit: ar_prm_clk_enable_v2");
    return rc;
}

static int32_t ar_prm_clk_enable(struct ar_clk_config *clks)
{
    int32_t rc = 0;
    audio_hw_clk_cfg_t *clk_enable = NULL;
    audio_hw_clk_cfg_req_param_t *clk_req;
    void *payload = NULL;
    size_t payload_size = 0;
    void *buff = NULL;
    uint32_t buff_size = 0;
    int32_t i;

    //csd2_mutex_lock(csd2_prm_ctx.lock);
    payload_size = sizeof(struct ar_prm_header) + sizeof(audio_hw_clk_cfg_req_param_t) + clks->num_clks * sizeof(audio_hw_clk_cfg_t);
    rc = ar_prm_alloc_packet(payload_size, &payload);
    if (rc)
    {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Error when allocating packet of size %d for CLK_v2!", payload_size);
        goto exit;
    }
    ((struct ar_prm_header *)payload)->param_id = PARAM_ID_RSC_AUDIO_HW_CLK;
    clk_req = (audio_hw_clk_cfg_req_param_t *)((uint8_t *)payload + sizeof(struct ar_prm_header));
    clk_req->num_clock_id = clks->num_clks;

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "clk req:num %d",clk_req->num_clock_id);

    clk_enable = (audio_hw_clk_cfg_t *)++clk_req;
    for (i=0;i<clks->num_clks;i++)
    {
        clk_enable[i].clock_id = clks->entries[i].id;
        clk_enable[i].clock_freq = clks->entries[i].freq;
        clk_enable[i].clock_attri = clks->entries[i].attri;
        clk_enable[i].clock_root = clks->entries[i].root;
    }
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "clk_v2 req:id %d", clk_enable[0].clock_id);
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "clk_v2 req:freq %d", clk_enable[0].clock_freq);
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "clk_v2 req:attri %d", clk_enable[0].clock_attri);
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "clk_v2 req:root %d", clk_enable[0].clock_root);

    rc = gsl_request_hw_rsc_custom_config(payload, payload_size, buff, &buff_size);
    if (rc)
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to request clk!");
exit:
    if (payload)
        ar_mem_free(payload);
   // csd2_mutex_unlock(csd2_prm_ctx.lock);
    return rc;
}

static int32_t ar_prm_clk_disable_v2(struct ar_prm_clk_disable *clks)
{
    int32_t rc = 0;
    audio_hw_clk_rel_cfg_t *clk_disable = NULL;
    audio_hw_clk_cfg_req_param_t *clk_req;
    void *payload = NULL;
    size_t payload_size = 0;
    void *buff = NULL;
    uint32_t buff_size = 0;
    int32_t i;

    //csd2_mutex_lock(csd2_prm_ctx.lock);
    payload_size = sizeof(struct ar_prm_header) + sizeof(audio_hw_clk_cfg_req_param_t) + clks->num_clks * sizeof(audio_hw_clk_rel_cfg_t);
    rc = ar_prm_alloc_packet(payload_size, &payload);
    if (rc)
    {
        AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Error when allocating packet of size %d!", payload_size);
        goto exit;
    }
    ((struct ar_prm_header *)payload)->param_id = PARAM_ID_RSC_AUDIO_HW_CLK_V2;
    clk_req = (audio_hw_clk_cfg_req_param_t *)((uint8_t *)payload + sizeof(struct ar_prm_header));
    clk_req->num_clock_id  =  clks->num_clks;
    clk_disable = (audio_hw_clk_rel_cfg_t *)++clk_req;
    for (i=0;i<clks->num_clks;i++)
    {
        clk_disable[i].clock_id = clks->clk_ids[0];
    }

    //if (csd2_handle_is_type_ssr(CSD2_MODULE_TYPE_AUDIO_SESSION) == FALSE) {
        rc = gsl_release_hw_rsc_custom_config(payload, payload_size, buff, &buff_size);
        if (rc)
            AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Failed to release clk!");
   // }

exit:
    if (payload)
        ar_mem_free(payload);
    //csd2_mutex_unlock(csd2_prm_ctx.lock);
    return rc;
}

int32_t ar_prm_set_default_mclk()
{
    uint32_t rc = 0;
    struct ar_clk_config* clk_config = NULL;
    int32_t c = 0;
    uint32_t clock_id = CLOCK_ID_MCLK_1;
    uint32_t clock_freq = 12288000;
    uint32_t clock_root = CLOCK_ROOT_DEFAULT;
    uint32_t clock_attr = CLOCK_ATTRIBUTE_COUPLE_NO;
    uint32_t mux_index = 0;
    uint32_t m_value = 0;
    uint32_t n_value = 0;
    uint32_t d_value = 0;
    int32_t opt_cnt = 0;
    uint32_t session_handle = 0;
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Enter: ar_prm_set_default_mclk");

    clk_config = calloc(1, sizeof(struct ar_clk_config) + 1 *sizeof(struct ar_clk_config_entry));
    if(NULL == clk_config)
    {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to allocate memory for clock config");
        rc = -ENOMEM;
        goto end;
    }

    clk_config->num_clks = 1;
    clk_config->entries[0].id      = clock_id;
    clk_config->entries[0].freq    = clock_freq;
    clk_config->entries[0].root    = clock_root;
    clk_config->entries[0].attri   = clock_attr;
    clk_config->entries[0].mux_index   = mux_index;
    clk_config->entries[0].divider_2x  = 0;
    clk_config->entries[0].counter_m_value = m_value;
    clk_config->entries[0].counter_n_value = n_value;
    clk_config->entries[0].d_value = d_value;

    rc = ar_prm_clk_enable_v2(clk_config);
    //rc = ar_prm_clk_enable(clk_config);
    if(rc)
    {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to set default mclk");
        rc = -EINVAL;
        goto end;
    }

end:
   if(clk_config)
        ar_mem_free(clk_config);

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE,"Exit: ar_prm_set_default_mclk");
    return rc;
}

int32_t ar_prm_release_default_mclk()
{
    uint32_t rc = 0;
    struct ar_prm_clk_disable* clk_release = NULL;
    int32_t c = 0;
    uint32_t clock_id = 0;
    int32_t opt_cnt = 0;
    uint32_t session_handle = 0;

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE,"Enter: ar_prm_release_default_mclk");
    clk_release = ar_mem_zalloc(sizeof(struct ar_prm_clk_disable) + 1 *sizeof(uint32_t));

    clk_release->num_clks = 1;
    clk_release->clk_ids[0] = clock_id;

    rc = ar_prm_clk_disable_v2(clk_release);
    if(rc)
    {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to release default mclk");
        rc = -EINVAL;
        goto end;
    }

end:
    if(clk_release)
        ar_mem_free(clk_release);

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE,"Exit: ar_prm_release_default_mclk");
    return rc;
}
