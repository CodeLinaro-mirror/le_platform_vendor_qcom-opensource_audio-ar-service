/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <errno.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pcm_logging_util.h"

#define ALIGN_8_BYTES(x) (((x) + 7U) & ~7U)

struct gsl_module_id_info_entry {
    uint32_t module_id;
    uint32_t module_iid;
};

struct gsl_tag_module_info_entry {
    uint32_t tag_id;
    uint32_t num_modules;
    struct gsl_module_id_info_entry module_entry[0];
};

struct gsl_tag_module_info {
    uint32_t num_tags;
    struct gsl_tag_module_info_entry tag_module_entry[0];
};

static int32_t parse_u32(const char *input, uint32_t *out);
static int32_t get_gkv_size(const struct pcm_logging_kvp *gkv, size_t *gkv_size);
static int32_t align_param_payload_size(uint32_t payload_size, size_t *param_size);

static int32_t get_gkv_size(const struct pcm_logging_kvp *gkv, size_t *gkv_size)
{
    if (!gkv || !gkv_size || (gkv->num_kvs == 0U) ||
        (gkv->num_kvs > PCM_LOGGING_MAX_KV_PAIRS)) {
        return -EINVAL;
    }

    *gkv_size = (size_t)gkv->num_kvs * sizeof(struct agm_key_value);
    return 0;
}

static int32_t align_param_payload_size(uint32_t payload_size, size_t *param_size)
{
    size_t raw_size = 0U;

    if (!param_size || (payload_size == 0U)) {
        return -EINVAL;
    }

    raw_size = sizeof(struct apm_module_param_data_t) + (size_t)payload_size;
    if (raw_size < (size_t)payload_size) {
        return -EINVAL;
    }

    if (raw_size > (((size_t)-1) - 7U)) {
        return -EINVAL;
    }

    *param_size = ALIGN_8_BYTES(raw_size);
    return 0;
}

static int32_t set_channel_mask_bit(uint32_t ch, uint32_t *group_words, uint32_t *group_mask)
{
    uint32_t group = 0;
    uint32_t bit = 0;

    if (!group_words || !group_mask) {
        return -EINVAL;
    }

    if ((ch == 0U) || (ch > 128U)) {
        return -EINVAL;
    }

    group = (ch - 1U) / 32U;
    bit = (ch - 1U) % 32U;
    if (group >= PCM_LOGGING_MAX_CHANNEL_MASK_GROUPS) {
        return -EINVAL;
    }

    *group_mask |= (1U << group);
    group_words[group] |= (1U << bit);
    return 0;
}

static int32_t generate_channel_masks(uint32_t ch_min, uint32_t ch_max, uint32_t *group_words,
                                      uint32_t *group_mask)
{
    uint32_t ch;
    int32_t rc = 0;

    if (!group_words || !group_mask) {
        return -EINVAL;
    }

    if ((ch_min == 0U) || (ch_max == 0U) || (ch_min > ch_max) || (ch_max > 128U)) {
        return -EINVAL;
    }

    *group_mask = 0U;
    memset(group_words, 0, PCM_LOGGING_MAX_CHANNEL_MASK_GROUPS * sizeof(uint32_t));

    for (ch = ch_min; ch <= ch_max; ch++) {
        rc = set_channel_mask_bit(ch, group_words, group_mask);
        if (rc) {
            return rc;
        }
    }

    return 0;
}

static int32_t generate_channel_masks_from_list(const uint32_t *channels, uint32_t num_channels,
                                                uint32_t *group_words, uint32_t *group_mask)
{
    uint32_t i = 0;
    int32_t rc = 0;

    if (!channels || !num_channels || !group_words || !group_mask) {
        return -EINVAL;
    }

    *group_mask = 0U;
    memset(group_words, 0, PCM_LOGGING_MAX_CHANNEL_MASK_GROUPS * sizeof(uint32_t));

    for (i = 0; i < num_channels; i++) {
        rc = set_channel_mask_bit(channels[i], group_words, group_mask);
        if (rc) {
            return rc;
        }
    }

    return 0;
}

static int32_t parse_channel_mask_words(const char *input, uint32_t *mask_words,
                                        uint32_t max_words, uint32_t *num_words)
{
    char *temp = NULL;
    char *saveptr = NULL;
    char *token = NULL;
    uint32_t index = 0;
    int32_t rc = 0;

    if (!input || !mask_words || !num_words || !max_words) {
        return -EINVAL;
    }

    temp = strdup(input);
    if (!temp) {
        return -ENOMEM;
    }

    token = strtok_r(temp, ",:| \t\r\n", &saveptr);
    while (token) {
        uint32_t value = 0;

        if (index >= max_words) {
            rc = -E2BIG;
            goto done;
        }

        rc = parse_u32(token, &value);
        if (rc) {
            goto done;
        }

        mask_words[index++] = value;
        token = strtok_r(NULL, ",:| \t\r\n", &saveptr);
    }

    if (index == 0U) {
        rc = -EINVAL;
        goto done;
    }

    *num_words = index;

done:
    free(temp);
    return rc;
}

static int32_t build_v2_words_from_group_words(const uint32_t *group_words,
                                               uint32_t group_mask,
                                               uint32_t *words,
                                               uint32_t words_capacity,
                                               uint32_t *payload_words)
{
    uint32_t iter = 0;
    uint32_t group = 0;

    if (!group_words || !words || !payload_words || (words_capacity < 3U)) {
        return -EINVAL;
    }

    words[iter++] = 0U;
    words[iter++] = group_mask;

    for (group = 0; group < PCM_LOGGING_MAX_CHANNEL_MASK_GROUPS; group++) {
        if (group_mask & (1U << group)) {
            if (iter >= words_capacity) {
                return -EINVAL;
            }
            words[iter++] = group_words[group];
        }
    }

    if (iter >= words_capacity) {
        return -EINVAL;
    }

    words[iter++] = 0U;
    *payload_words = iter;
    return 0;
}

static int32_t parse_u32(const char *input, uint32_t *out)
{
    char *end = NULL;
    unsigned long value = 0;

    if (!input || !out) {
        return -EINVAL;
    }

    errno = 0;
    value = strtoul(input, &end, 0);
    if ((errno != 0) || (end == input) || (*end != '\0') || (value > UINT32_MAX)) {
        return -EINVAL;
    }

    *out = (uint32_t)value;
    return 0;
}

static int32_t parse_channel_min_max(const char *input, uint32_t *ch_min, uint32_t *ch_max)
{
    char *temp = NULL;
    char *saveptr = NULL;
    char *first = NULL;
    char *second = NULL;
    char *third = NULL;
    int32_t rc = 0;

    if (!input || !ch_min || !ch_max) {
        return -EINVAL;
    }

    temp = strdup(input);
    if (!temp) {
        return -ENOMEM;
    }

    first = strtok_r(temp, "(),", &saveptr);
    second = strtok_r(NULL, "(),", &saveptr);
    if (!first || !second) {
        rc = -EINVAL;
        goto done;
    }

    rc = parse_u32(first, ch_min);
    if (rc) {
        goto done;
    }

    rc = parse_u32(second, ch_max);
    if (rc) {
        goto done;
    }

    third = strtok_r(NULL, "(),", &saveptr);
    if (third) {
        rc = -EINVAL;
    }

done:
    free(temp);
    return rc;
}

static bool is_explicit_list_syntax(const char *input)
{
    const char *ptr = input;

    if (!input) {
        return false;
    }

    while (*ptr != '\0') {
        if (!isspace((unsigned char)*ptr)) {
            return ((*ptr == '{') || (*ptr == '['));
        }
        ptr++;
    }

    return false;
}

static bool is_explicit_range_syntax(const char *input)
{
    const char *ptr = input;

    if (!input) {
        return false;
    }

    while (*ptr != '\0') {
        if (!isspace((unsigned char)*ptr)) {
            return (*ptr == '(');
        }
        ptr++;
    }

    return false;
}

static int32_t parse_channel_list_string(const char *input, uint32_t *channels, uint32_t max_channels,
                                         uint32_t *num_channels)
{
    char *temp = NULL;
    char *saveptr = NULL;
    char *token = NULL;
    uint32_t index = 0;
    int32_t rc = 0;

    if (!input || !channels || !num_channels || !max_channels) {
        return -EINVAL;
    }

    temp = strdup(input);
    if (!temp) {
        return -ENOMEM;
    }

    token = strtok_r(temp, "(),{}[] \t\r\n", &saveptr);
    while (token) {
        uint32_t value = 0;

        if (index >= max_channels) {
            rc = -E2BIG;
            goto done;
        }

        rc = parse_u32(token, &value);
        if (rc || (value == 0U) || (value > 128U)) {
            rc = -EINVAL;
            goto done;
        }

        channels[index++] = value;
        token = strtok_r(NULL, "(),{}[] \t\r\n", &saveptr);
    }

    if (index == 0U) {
        rc = -EINVAL;
        goto done;
    }

    *num_channels = index;

done:
    free(temp);
    return rc;
}

static int32_t parse_gkv_string(const char *input, struct pcm_logging_kvp *kvp)
{
    char *temp = NULL;
    char *saveptr = NULL;
    char *token = NULL;
    uint32_t index = 0;
    int32_t rc = 0;

    if (!input || !kvp) {
        return -EINVAL;
    }

    memset(kvp, 0, sizeof(*kvp));

    temp = strdup(input);
    if (!temp) {
        return -ENOMEM;
    }

    token = strtok_r(temp, "(),", &saveptr);
    while (token) {
        uint32_t key = 0;
        uint32_t value = 0;

        if (index >= PCM_LOGGING_MAX_KV_PAIRS) {
            rc = -E2BIG;
            goto done;
        }

        rc = parse_u32(token, &key);
        if (rc) {
            goto done;
        }

        token = strtok_r(NULL, "(),", &saveptr);
        if (!token) {
            rc = -EINVAL;
            goto done;
        }

        rc = parse_u32(token, &value);
        if (rc) {
            goto done;
        }

        kvp->kv[index].key = key;
        kvp->kv[index].value = value;
        index++;

        token = strtok_r(NULL, "(),", &saveptr);
    }

    kvp->num_kvs = index;

    if (kvp->num_kvs == 0U) {
        rc = -EINVAL;
    }

done:
    free(temp);
    return rc;
}

static int32_t agm_set_data_logging_payload(const struct pcm_logging_kvp *gkv,
                                            uint32_t tag,
                                            uint32_t param_id,
                                            const void *payload,
                                            uint32_t payload_size)
{
    struct apm_module_param_data_t *header = NULL;
    /* CLI may call this helper multiple times in one run (-m and -c). */
    static bool agm_initialized = false;
    uint8_t *meta_payload = NULL;
    uint8_t *tag_info_payload = NULL;
    uint8_t *param_payload = NULL;
    uint64_t sess_handle = 0;
    bool session_opened = false;
    uint32_t session_id = 0;
    uint32_t miid = 0;
    uint8_t *entry_ptr = NULL;
    uint8_t *tag_info_end = NULL;
    size_t tag_info_size = 0;
    size_t gkv_size = 0;
    size_t meta_size = 0;
    size_t param_size = 0;
    size_t remaining_size = 0;
    size_t offset = 0;
    uint32_t i = 0;
    int32_t rc = 0;

    if (!gkv || !payload || (payload_size == 0U)) {
        return -EINVAL;
    }

    rc = get_gkv_size(gkv, &gkv_size);
    if (rc) {
        return rc;
    }

    if (gkv_size > (((size_t)-1) - (2U * sizeof(uint32_t)))) {
        return -EINVAL;
    }
    meta_size = sizeof(uint32_t) + gkv_size + sizeof(uint32_t);
    if (meta_size > UINT32_MAX) {
        return -EINVAL;
    }

    rc = align_param_payload_size(payload_size, &param_size);
    if (rc) {
        return rc;
    }

    if (!agm_initialized) {
        rc = agm_init();
        if (rc) {
            goto done;
        }
        agm_initialized = true;
    }

    meta_payload = (uint8_t *)calloc(1, meta_size);
    if (!meta_payload) {
        rc = -ENOMEM;
        goto done;
    }
    memcpy(meta_payload, &gkv->num_kvs, sizeof(uint32_t));
    if (gkv_size > 0U) {
        memcpy(meta_payload + sizeof(uint32_t), gkv->kv, gkv_size);
    }
    /* Minimal metadata blob: [num_gkv][gkv...][num_ckv=0] */
    *(uint32_t *)(meta_payload + sizeof(uint32_t) + gkv_size) = 0U;

    session_id = 0x60000000U | ((uint32_t)getpid() & 0x0000FFFFU);

    rc = agm_session_set_metadata(session_id, (uint32_t)meta_size, meta_payload);
    if (rc) {
        goto done;
    }

    rc = agm_session_open(session_id, AGM_SESSION_NO_CONFIG, &sess_handle);
    if (rc) {
        goto done;
    }
    session_opened = true;

    rc = agm_session_aif_get_tag_module_info(session_id, UINT_MAX, NULL, &tag_info_size);
    if (rc || (tag_info_size < sizeof(struct gsl_tag_module_info))) {
        rc = -EINVAL;
        goto done;
    }

    tag_info_payload = (uint8_t *)calloc(1, tag_info_size);
    if (!tag_info_payload) {
        rc = -ENOMEM;
        goto done;
    }

    rc = agm_session_aif_get_tag_module_info(session_id, UINT_MAX,
                                             tag_info_payload, &tag_info_size);
    if (rc) {
        goto done;
    }

    entry_ptr = (uint8_t *)((struct gsl_tag_module_info *)tag_info_payload)->tag_module_entry;
    tag_info_end = tag_info_payload + tag_info_size;
    printf("pcm_logging_util: tags available for given gkv (%u):\n",
           ((struct gsl_tag_module_info *)tag_info_payload)->num_tags);
    for (i = 0; i < ((struct gsl_tag_module_info *)tag_info_payload)->num_tags; i++) {
        struct gsl_tag_module_info_entry *entry = NULL;
        size_t modules_size = 0;
        size_t entry_size = 0;

        if ((entry_ptr < tag_info_payload) || (entry_ptr > tag_info_end)) {
            rc = -EINVAL;
            goto done;
        }

        remaining_size = (size_t)(tag_info_end - entry_ptr);
        if (remaining_size < sizeof(struct gsl_tag_module_info_entry)) {
            rc = -EINVAL;
            goto done;
        }

        entry = (struct gsl_tag_module_info_entry *)entry_ptr;
        modules_size = (size_t)entry->num_modules *
                       sizeof(struct gsl_module_id_info_entry);
        if ((entry->num_modules != 0U) &&
            ((modules_size / sizeof(struct gsl_module_id_info_entry)) !=
             (size_t)entry->num_modules)) {
            rc = -EINVAL;
            goto done;
        }

        entry_size = sizeof(*entry) + modules_size;
        if (entry_size < modules_size) {
            rc = -EINVAL;
            goto done;
        }
        if (entry_size > remaining_size) {
            rc = -EINVAL;
            goto done;
        }

        printf("  tag=0x%x num_modules=%u\n", entry->tag_id, entry->num_modules);
        if ((entry->tag_id == tag) && (entry->num_modules > 0U)) {
            miid = entry->module_entry[0].module_iid;
            break;
        }

        entry_ptr += entry_size;
    }

    if (miid == 0U) {
        rc = -ENOENT;
        goto done;
    }

    param_payload = (uint8_t *)calloc(1, param_size);
    if (!param_payload) {
        rc = -ENOMEM;
        goto done;
    }

    header = (struct apm_module_param_data_t *)param_payload;
    header->module_instance_id = miid;
    header->param_id = param_id;
    header->param_size = payload_size;
    header->error_code = 0U;

    offset = sizeof(struct apm_module_param_data_t);
    memcpy(param_payload + offset, payload, payload_size);

    rc = agm_session_set_params(session_id, param_payload, param_size);

done:
    if (session_opened) {
        (void)agm_session_close(sess_handle);
    }
    free(param_payload);
    free(tag_info_payload);
    free(meta_payload);
    return rc;
}

static int32_t agm_pcm_logging_set_v1(const struct pcm_logging_kvp *gkv, uint32_t tag, uint32_t ch_mask)
{
    data_logging_select_channels_t param = {
        .mode = 0U,
        .channel_index_mask = ch_mask,
        .channel_type_mask_lsw = 0xFFFFFFFEU,
        .channel_type_mask_msw = 0xFFFF0007U,
    };

    return agm_set_data_logging_payload(gkv, tag,
                                        PARAM_ID_DATA_LOGGING_SELECT_CHANNELS,
                                        &param, sizeof(param));
}

static int32_t agm_pcm_logging_set_module_enable(const struct pcm_logging_kvp *gkv, uint32_t tag,
                                                 uint32_t enable)
{
    struct agm_acdb_tunnel_param *acdb_payload = NULL;
    struct apm_module_param_data_t *header = NULL;
    data_logging_module_enable_t param = {
        .enable = enable,
    };
    size_t gkv_size = 0U;
    size_t module_param_size = 0U;
    size_t blob_size = 0U;
    size_t total_size = 0U;
    uint8_t *blob_ptr = NULL;
    int32_t rc = 0;

    rc = get_gkv_size(gkv, &gkv_size);
    if (rc) {
        return rc;
    }

    /*
     * Apply module enable over the runtime path first.
     * ACDB tunnel update is intentionally gated on runtime success.
     */
    rc = agm_set_data_logging_payload(gkv, tag,
                                      PARAM_ID_MODULE_ENABLE,
                                      &param, sizeof(param));
    if (rc) {
        /* Do not persist to ACDB when runtime apply fails. */
        return rc;
    }

    rc = align_param_payload_size(sizeof(param), &module_param_size);
    if (rc) {
        return rc;
    }

    if (gkv_size > (((size_t)-1) - module_param_size)) {
        return -EINVAL;
    }
    blob_size = gkv_size + module_param_size;
    if (blob_size > UINT32_MAX) {
        return -EINVAL;
    }
    if (blob_size > (((size_t)-1) - sizeof(struct agm_acdb_tunnel_param))) {
        return -EINVAL;
    }
    total_size = sizeof(struct agm_acdb_tunnel_param) + blob_size;

    acdb_payload = (struct agm_acdb_tunnel_param *)calloc(1, total_size);
    if (!acdb_payload) {
        return -ENOMEM;
    }

    acdb_payload->isTKV = true;
    acdb_payload->tag = tag;
    acdb_payload->num_gkvs = gkv->num_kvs;
    acdb_payload->num_kvs = 0U;
    acdb_payload->blob_size = (uint32_t)blob_size;

    memcpy(acdb_payload->blob, gkv->kv, gkv_size);

    blob_ptr = acdb_payload->blob + gkv_size;
    header = (struct apm_module_param_data_t *)blob_ptr;
    /* ACDB tunnel translates this field from tag -> MIID internally. */
    header->module_instance_id = tag;
    header->param_id = PARAM_ID_MODULE_ENABLE;
    header->param_size = sizeof(param);
    header->error_code = 0U;
    memcpy(blob_ptr + sizeof(*header), &param, sizeof(param));

    /* Runtime apply succeeded, now mirror it over ACDB tunnel. */
    rc = agm_set_params_to_acdb_tunnel(acdb_payload, total_size);
    free(acdb_payload);
    return rc;
}

static int32_t agm_pcm_logging_set_v2(const struct pcm_logging_kvp *gkv, uint32_t tag, uint32_t ch_min, uint32_t ch_max)
{
    uint32_t words[PCM_LOGGING_MAX_V2_WORDS] = {0};
    uint32_t group_words[PCM_LOGGING_MAX_CHANNEL_MASK_GROUPS] = {0};
    uint32_t group_mask = 0;
    uint32_t payload_words = 0;
    int32_t rc = 0;

    rc = generate_channel_masks(ch_min, ch_max, group_words, &group_mask);
    if (rc) {
        return rc;
    }

    rc = build_v2_words_from_group_words(group_words, group_mask, words,
                                         PCM_LOGGING_MAX_V2_WORDS, &payload_words);
    if (rc) {
        return rc;
    }

    return agm_set_data_logging_payload(gkv, tag,
                                        PARAM_ID_DATA_LOGGING_SELECT_CHANNELS_V2,
                                        words, payload_words * sizeof(uint32_t));
}

static int32_t agm_pcm_logging_set_v2_list(const struct pcm_logging_kvp *gkv, uint32_t tag,
                                           const uint32_t *channels, uint32_t num_channels)
{
    uint32_t words[PCM_LOGGING_MAX_V2_WORDS] = {0};
    uint32_t group_words[PCM_LOGGING_MAX_CHANNEL_MASK_GROUPS] = {0};
    uint32_t group_mask = 0;
    uint32_t payload_words = 0;
    int32_t rc = 0;

    rc = generate_channel_masks_from_list(channels, num_channels, group_words, &group_mask);
    if (rc) {
        return rc;
    }

    rc = build_v2_words_from_group_words(group_words, group_mask, words,
                                         PCM_LOGGING_MAX_V2_WORDS, &payload_words);
    if (rc) {
        return rc;
    }

    return agm_set_data_logging_payload(gkv, tag,
                                        PARAM_ID_DATA_LOGGING_SELECT_CHANNELS_V2,
                                        words, payload_words * sizeof(uint32_t));
}

static int32_t agm_pcm_logging_set_v2_mask_words(const struct pcm_logging_kvp *gkv, uint32_t tag,
                                                 const uint32_t *mask_words, uint32_t num_mask_words)
{
    uint32_t words[PCM_LOGGING_MAX_V2_WORDS] = {0};
    uint32_t group_words[PCM_LOGGING_MAX_CHANNEL_MASK_GROUPS] = {0};
    uint32_t group_mask = 0U;
    uint32_t payload_words = 0U;
    uint32_t i = 0U;
    int32_t rc = 0;

    if (!mask_words || !num_mask_words || (num_mask_words > PCM_LOGGING_MAX_CHANNEL_MASK_GROUPS)) {
        return -EINVAL;
    }

    for (i = 0; i < num_mask_words; i++) {
        group_words[i] = mask_words[i];
        if (mask_words[i] != 0U) {
            group_mask |= (1U << i);
        }
    }

    rc = build_v2_words_from_group_words(group_words, group_mask, words,
                                         PCM_LOGGING_MAX_V2_WORDS, &payload_words);
    if (rc) {
        return rc;
    }

    return agm_set_data_logging_payload(gkv, tag,
                                        PARAM_ID_DATA_LOGGING_SELECT_CHANNELS_V2,
                                        words, payload_words * sizeof(uint32_t));
}

static void print_usage(void)
{
    const char *usage =
    "\n"
    "Usage: pcm_logging_util -g gkv -t tag [-m 0|1] [-c channel_cfg] [-v 1|2]\n"
    "\n"
    "c: channel config\n"
    "   V1 (-v 1): channel bitmask (ex: 0xFFFFFFAA)\n"
    "   V2 (default): range \"(min,max)\", channel list \"{ch1,ch2,...}\", or mask words \"0xW0[,0xW1,0xW2,0xW3]\"\n"
    "g: gkv: ((k1,v1),(k2,v2),(k3,v3),(k4,v4))\n"
    "   spf pcm logging configuration (required)\n"
    "h: usage\n"
    "m: module enable control (0: disable, 1: enable)\n"
    "t: tag ID for data logging module\n"
    "v: By default v represents version 2, set v to 1 for backward compatibility\n"
    "   Provide at least one of -m or -c\n"
    "\n"
    "Usage Example:\n"
    "\tpcm_logging_util -g \"((0xa1000000,0xa1000001),(0xab000000,0x1),(0xff000000,0x0),(0xac000000,0xac000002),(0xa2000000,0xa200000b))\" -t 0xC00000F7 -m 0\n"
    "\tpcm_logging_util -g \"((0xa1000000,0xa1000001),(0xab000000,0x1),(0xff000000,0x0),(0xac000000,0xac000002),(0xa2000000,0xa200000b))\" -t 0xC00000F7 -m 1\n"
    "\tpcm_logging_util -g \"((0xa1000000,0xa1000001),(0xab000000,0x1),(0xff000000,0x0),(0xac000000,0xac000002),(0xa2000000,0xa200000b))\" -t 0xC00000F7 -c \"(0x18,0x24)\"\n"
    "\tpcm_logging_util -g \"((0xa1000000,0xa1000001),(0xab000000,0x1),(0xff000000,0x0),(0xac000000,0xac000002),(0xa2000000,0xa200000b))\" -t 0xC00000F7 -c \"{0x1,0x3,0x6}\"\n"
    "\tpcm_logging_util -g \"((0xa1000000,0xa1000001),(0xab000000,0x1),(0xff000000,0x0),(0xac000000,0xac000002),(0xa2000000,0xa200000b))\" -t 0xC00000F7 -c \"0x0000002D\"\n"
    "\tpcm_logging_util -g \"((0xa1000000,0xa1000001),(0xab000000,0x1),(0xff000000,0x0),(0xac000000,0xac000002),(0xa2000000,0xa200000b))\" -t 0xC00000F7 -c \"0x0000002D,0x0,0x0,0x0\"\n"
    "\tpcm_logging_util -v 1 -g \"((0xa1000000,0xa1000001),(0xab000000,0x1),(0xff000000,0x0),(0xac000000,0xac000002),(0xa2000000,0xa200000b))\" -t 0xC00000F7 -c 0xFFFFFFAA\n"
    "\n";

    printf("%s\n", usage);
}

int main(int argc, char **argv)
{
    struct pcm_logging_kvp gkv = {0};
    uint32_t ch_list[128] = {0};
    uint32_t ch_mask_words[PCM_LOGGING_MAX_CHANNEL_MASK_GROUPS] = {0};
    const char *gkv_arg = NULL;
    const char *ch_arg = NULL;
    const char *module_enable_arg = NULL;
    int32_t rc = 0;
    int opt = 0;
    bool got_g = false;
    bool got_t = false;
    bool got_c = false;
    bool got_m = false;
    uint32_t tag = 0U;
    uint32_t module_enable = 0U;
    uint32_t ch_mask = 0xFFU;
    uint32_t ch_min = 0U;
    uint32_t ch_max = 0xFFU;
    uint32_t num_mask_words = 0U;
    uint32_t num_channels = 0U;
    int32_t version = SELECTIVE_LOGGING_V2;

    while ((opt = getopt(argc, argv, "c:g:m:v:ht:")) != -1) {
        switch (opt) {
        case 'c':
            ch_arg = optarg;
            got_c = true;
            break;
        case 'g':
            gkv_arg = optarg;
            got_g = true;
            break;
        case 'h':
            print_usage();
            return 0;
        case 't':
            rc = parse_u32(optarg, &tag);
            if (rc) {
                printf("pcm_logging_util: invalid tag '%s'\n", optarg);
                return -EINVAL;
            }
            got_t = true;
            break;
        case 'm':
            module_enable_arg = optarg;
            got_m = true;
            break;
        case 'v':
            version = (int32_t)strtol(optarg, NULL, 0);
            break;
        default:
            print_usage();
            return -EINVAL;
        }
    }

    if (!got_t || !got_g || (!got_c && !got_m)) {
        printf("Error: SPF mode requires -g, -t and at least one of -m or -c options.\n");
        print_usage();
        return -EINVAL;
    }

    rc = parse_gkv_string(gkv_arg, &gkv);
    if (rc) {
        printf("pcm_logging_util: invalid gkv format (rc=%d)\n", rc);
        return rc;
    }

    printf("gkv:\n");
    for (uint32_t i = 0; i < gkv.num_kvs; i++) {
        printf("(0x%x, 0x%x)\n", gkv.kv[i].key, gkv.kv[i].value);
    }

    if (got_m) {
        rc = parse_u32(module_enable_arg, &module_enable);
        if (rc || (module_enable > 1U)) {
            printf("pcm_logging_util: invalid module enable state '%s' (expected 0 or 1)\n",
                   module_enable_arg);
            return -EINVAL;
        }

        printf("tag=0x%x, module_enable=%u\n", tag, module_enable);
        rc = agm_pcm_logging_set_module_enable(&gkv, tag, module_enable);
        if (rc) {
            goto done;
        }
    }

    if (got_c) {
        if (version == SELECTIVE_LOGGING_V1) {
            rc = parse_u32(ch_arg, &ch_mask);
            if (rc) {
                printf("pcm_logging_util: invalid channel mask '%s'\n", ch_arg);
                return rc;
            }

            printf("tag=0x%x, ch_mask=0x%x\n", tag, ch_mask);
            rc = agm_pcm_logging_set_v1(&gkv, tag, ch_mask);
        } else if (version == SELECTIVE_LOGGING_V2) {
            if (is_explicit_range_syntax(ch_arg)) {
                rc = parse_channel_min_max(ch_arg, &ch_min, &ch_max);
                if (rc) {
                    printf("pcm_logging_util: invalid v2 range '%s'\n", ch_arg);
                    return rc;
                }
                printf("tag=0x%x, ch_min=0x%x ch_max=0x%x\n", tag, ch_min, ch_max);
                rc = agm_pcm_logging_set_v2(&gkv, tag, ch_min, ch_max);
            } else if (is_explicit_list_syntax(ch_arg)) {
                rc = parse_channel_list_string(ch_arg, ch_list,
                                               (uint32_t)(sizeof(ch_list) / sizeof(ch_list[0])),
                                               &num_channels);
                if (rc) {
                    printf("pcm_logging_util: invalid v2 channel list '%s'\n", ch_arg);
                    return rc;
                }

                printf("tag=0x%x, ch_list=", tag);
                for (uint32_t i = 0; i < num_channels; i++) {
                    printf("%s0x%x", (i == 0U) ? "" : ",", ch_list[i]);
                }
                printf("\n");
                rc = agm_pcm_logging_set_v2_list(&gkv, tag, ch_list, num_channels);
            } else {
                rc = parse_channel_mask_words(ch_arg, ch_mask_words,
                                              PCM_LOGGING_MAX_CHANNEL_MASK_GROUPS,
                                              &num_mask_words);
                if (rc) {
                    printf("pcm_logging_util: invalid v2 mask words '%s'\n", ch_arg);
                    printf("pcm_logging_util: use \"(min,max)\", \"{ch1,ch2,...}\", or \"0xW0[,0xW1,0xW2,0xW3]\"\n");
                    return rc;
                }

                printf("tag=0x%x, ch_mask_words=", tag);
                for (uint32_t i = 0; i < num_mask_words; i++) {
                    printf("%s0x%x", (i == 0U) ? "" : ",", ch_mask_words[i]);
                }
                printf("\n");
                rc = agm_pcm_logging_set_v2_mask_words(&gkv, tag, ch_mask_words, num_mask_words);
            }
        } else {
            printf("pcm_logging_util: invalid version %d\n", version);
            return -EINVAL;
        }
    }

done:
    if (rc == 0) {
        printf("pcm_logging_util: set for spf succeeded\n");
    } else {
        printf("pcm_logging_util: set for spf failed [rc=%d / 0x%x]\n", rc, (uint32_t)rc);
    }

    return rc;
}
