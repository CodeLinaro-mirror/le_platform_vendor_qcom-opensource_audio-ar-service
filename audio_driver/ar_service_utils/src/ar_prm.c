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
#include <ctype.h>

#define AR_PRM_MCLK_CFG_FILE  "/etc/audio_oem_cfg/ar_prm_mclk.cfg"
#define AR_PRM_MCLK_MAX_NUM   8
#define CLOCK_FREQS_DEFAULT   12288000

#define AR_PRM_GET_SUBKEY_VAL(s, skey, v)       \
    {                                           \
        v = NULL;                               \
        int sklen = strlen(skey);               \
        if (strncmp(s, skey, sklen) == 0) {     \
            if (s[sklen] == ':' || s[sklen] == '=') \
                v = &s[sklen + 1];              \
        }                                       \
    }

#define AR_PRM_GET_VAL_INT(sv, v, num)          \
    {                                           \
        if (sv != NULL) {                       \
            while (sv[0] != '\0') {             \
                if (isdigit(*sv)) {             \
                    int val = strtol(sv, &sv, 10); \
                    *v++ = val;                 \
                    ++num;                      \
                } else {                        \
                    sv++;                       \
                }                               \
            }                                   \
        }                                       \
    }

struct ar_prm_mclk_cfg {
    int      num_clks;
    uint32_t clock_ids[AR_PRM_MCLK_MAX_NUM];
    uint32_t clock_freqs[AR_PRM_MCLK_MAX_NUM];
    uint32_t clock_attris[AR_PRM_MCLK_MAX_NUM];
    uint32_t clock_roots[AR_PRM_MCLK_MAX_NUM];
};

static int ar_prm_load_mclk_cfg(struct ar_prm_mclk_cfg *cfg)
{
    int id_num = 0;
    int freq_num = 0;
    int attri_num = 0;
    int root_num = 0;
    bool in_section = false;
    FILE *fp = NULL;
    char buffer[64];

    memset(cfg, 0, sizeof(*cfg));

    fp = fopen(AR_PRM_MCLK_CFG_FILE, "r");
    if (!fp) {
        AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE,
            "ar_prm_mclk.cfg not found, skip mclk");
        return -1;
    }

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        char *sv = NULL;
        /* only process mclk_ctrl section in cfg file */
        if (strncmp(buffer, "[mclk_ctrl]", 11) == 0) {
            in_section = true;
            continue;
        }
        if (buffer[0] == '[') {
            in_section = false;
            continue;
        }
        if (!in_section)
            continue;

        AR_PRM_GET_SUBKEY_VAL(buffer, "clock_id", sv);
        if (sv) {
            uint32_t *v = cfg->clock_ids;
            AR_PRM_GET_VAL_INT(sv, v, id_num);
            continue;
        }

        AR_PRM_GET_SUBKEY_VAL(buffer, "clock_freq", sv);
        if (sv) {
            uint32_t *v = cfg->clock_freqs;
            AR_PRM_GET_VAL_INT(sv, v, freq_num);
            continue;
        }

        AR_PRM_GET_SUBKEY_VAL(buffer, "clock_attri", sv);
        if (sv) {
            uint32_t *v = cfg->clock_attris;
            AR_PRM_GET_VAL_INT(sv, v, attri_num);
            continue;
        }

        AR_PRM_GET_SUBKEY_VAL(buffer, "clock_root", sv);
        if (sv) {
            uint32_t *v = cfg->clock_roots;
            AR_PRM_GET_VAL_INT(sv, v, root_num);
            continue;
        }
    }

    fclose(fp);
    cfg->num_clks = id_num;

    /*Use clk_id as reference. If any fields are missing in the config file, fill them with default values.*/
    for (int i = 0; i < id_num; i++) {
        if (i >= freq_num)
            cfg->clock_freqs[i]  = CLOCK_FREQS_DEFAULT;
        if (i >= attri_num)
            cfg->clock_attris[i] = CLOCK_ATTRIBUTE_COUPLE_NO;
        if (i >= root_num)
            cfg->clock_roots[i]  = CLOCK_ROOT_DEFAULT;
    }

    return cfg->num_clks;
}

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

    rc = gsl_hw_rsc_init();
    if (rc)
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to init gsl_hw_rsc, rc: %d", rc);

    /* AR_ENOTREADY (10): DSP HW-RSC channel may not be ready immediately after
     * SSR. Retry up to 5 times with a 50 ms gap (250 ms total budget). */
    for (int retries = 0; retries < 5; retries++) {
        rc = gsl_request_hw_rsc_custom_config(payload, payload_size, buff, &buff_size);
        if (rc != 10)
            break;
        AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE,
            "gsl_request_hw_rsc_custom_config busy (rc=10), retry %d/5", retries + 1);
        usleep(50000);
    }
    if (rc)
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to request clk_v2!");
exit:
    if (payload)
        ar_mem_free(payload);

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Exit, rc: %d\n", rc);
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
    struct ar_prm_mclk_cfg mclk_cfg;
    int num_clks;

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Enter: ar_prm_set_default_mclk");

    num_clks = ar_prm_load_mclk_cfg(&mclk_cfg);
    if (num_clks <= 0) {
        AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "no mclk configured, skip enable");
        rc = -EINVAL;
        goto end;
    }

    clk_config = calloc(1, sizeof(struct ar_clk_config) + num_clks * sizeof(struct ar_clk_config_entry));
    if (clk_config == NULL) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to allocate memory for clock config");
        rc = -ENOMEM;
        goto end;
    }

    clk_config->num_clks = num_clks;
    for (int i = 0; i < num_clks; i++) {
        clk_config->entries[i].id              = mclk_cfg.clock_ids[i];
        clk_config->entries[i].freq            = mclk_cfg.clock_freqs[i];
        clk_config->entries[i].root            = mclk_cfg.clock_roots[i];
        clk_config->entries[i].attri           = mclk_cfg.clock_attris[i];
        clk_config->entries[i].mux_index       = 0;
        clk_config->entries[i].divider_2x      = 0;
        clk_config->entries[i].counter_m_value = 0;
        clk_config->entries[i].counter_n_value = 0;
        clk_config->entries[i].d_value         = 0;
    }

    rc = ar_prm_clk_enable_v2(clk_config);
    if (rc) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to set default mclk");
        rc = -EINVAL;
        goto end;
    }

end:
    if(clk_config)
        ar_mem_free(clk_config);

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Exit: ar_prm_set_default_mclk");
    return rc;
}

int32_t ar_prm_release_default_mclk()
{
    uint32_t rc = 0;
    struct ar_prm_clk_disable* clk_release = NULL;
    struct ar_prm_mclk_cfg mclk_cfg;
    int num_clks;

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Enter: ar_prm_release_default_mclk");

    num_clks = ar_prm_load_mclk_cfg(&mclk_cfg);
    if (num_clks <= 0) {
        AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "no mclk configured, skip release");
        rc = -EINVAL;
        goto end;
    }

    clk_release = ar_mem_zalloc(sizeof(struct ar_prm_clk_disable) + num_clks * sizeof(uint32_t));
    if (clk_release == NULL) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to allocate memory for clk_release");
        rc = -ENOMEM;
        goto end;
    }

    clk_release->num_clks = num_clks;
    for (int i = 0; i < num_clks; i++)
        clk_release->clk_ids[i] = mclk_cfg.clock_ids[i];

    rc = ar_prm_clk_disable_v2(clk_release);
    if (rc) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to release default mclk");
        rc = -EINVAL;
        goto end;
    }

end:
    if (clk_release)
        ar_mem_free(clk_release);

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Exit: ar_prm_release_default_mclk");
    return rc;
}
