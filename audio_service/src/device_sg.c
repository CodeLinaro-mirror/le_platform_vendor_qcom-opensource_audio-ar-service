/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <tinyalsa/asoundlib.h>
#include <sound/asound.h>
#include <agm/agm_api.h>
#include "audio_log_util.h"
#include "device_sg.h"

#define SAMPLINGRATE 0xA5000000
#define BITWIDTH     0xA6000000
#define GAIN         0xD2000000
#define DEVICERX     0xA2000000

struct device_config {
    char name[80];
    unsigned int rate;
    unsigned int ch;
    unsigned int bits;
    enum pcm_format format;
};

static int set_agm_audio_intf_metadata(struct mixer *mixer, char *intf_name, unsigned int dkv,
            int rate, int bitwidth, uint32_t val)
{
    char *control = "metadata";
    struct mixer_ctl *ctl;
    char *mixer_str;
    struct agm_key_value *gkv = NULL, *ckv = NULL;
    struct prop_data *prop = NULL;
    uint8_t *metadata = NULL;
    uint32_t num_gkv = 1, num_ckv = 3, num_props = 0;
    uint32_t gkv_size, ckv_size, prop_size, ckv_index = 0;
    int ctl_len = 0, offset = 0;
    int ret = 0;

    gkv_size = num_gkv * sizeof(struct agm_key_value);
    ckv_size = num_ckv * sizeof(struct agm_key_value);
    prop_size = sizeof(struct prop_data) + (num_props * sizeof(uint32_t));

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Enter, set_agm_audio_intf_metadata\n");

    metadata = calloc(1, sizeof(num_gkv) + sizeof(num_ckv) + gkv_size + ckv_size + prop_size);
    if (!metadata)
        return -ENOMEM;

    gkv = calloc(num_gkv, sizeof(struct agm_key_value));
    ckv = calloc(num_ckv, sizeof(struct agm_key_value));
    prop = calloc(1, prop_size);
    if (!gkv || !ckv || !prop) {
        free(ckv);
        free(gkv);
        free(prop);
        free(metadata);
        return -ENOMEM;
    }

    gkv[0].key = DEVICERX;
    gkv[0].value = dkv;

    ckv[ckv_index].key = SAMPLINGRATE;
    ckv[ckv_index].value = rate;
    ckv_index++;
    ckv[ckv_index].key = BITWIDTH;
    ckv[ckv_index].value = bitwidth;
    ckv_index++;
    ckv[ckv_index].key = GAIN;
    ckv[ckv_index].value = 0;

    prop->prop_id = 0;
    prop->num_values = num_props;

    memcpy(metadata, &num_gkv, sizeof(num_gkv));
    offset += sizeof(num_gkv);
    memcpy(metadata + offset, gkv, gkv_size);
    offset += gkv_size;
    memcpy(metadata + offset, &num_ckv, sizeof(num_ckv));
    offset += sizeof(num_ckv);
    memcpy(metadata + offset, ckv, ckv_size);
    offset += ckv_size;
    memcpy(metadata + offset, prop, prop_size);

    ctl_len = strlen(intf_name) + 1 + strlen(control) + 1;
    mixer_str = calloc(1, ctl_len);
    if (!mixer_str) {
        free(gkv);
        free(ckv);
        free(prop);
        free(metadata);
        return -ENOMEM;
    }
    snprintf(mixer_str, ctl_len, "%s %s", intf_name, control);

    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Invalid mixer control: %s\n", mixer_str);
        free(gkv);
        free(ckv);
        free(prop);
        free(metadata);
        free(mixer_str);
        return -ENOENT;
    }

    ret = mixer_ctl_set_array(ctl, metadata,
            sizeof(num_gkv) + sizeof(num_ckv) + gkv_size + ckv_size + prop_size);

    free(gkv);
    free(ckv);
    free(prop);
    free(metadata);
    free(mixer_str);

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Exit, set_agm_audio_intf_metadata\n");
    return ret;
}

static int connect_agm_audio_intf_to_stream(struct mixer *mixer, unsigned int device,
        char *intf_name, bool connect)
{
    char *stream = "PCM";
    char *control;
    char *mixer_str;
    struct mixer_ctl *ctl;
    int ctl_len = 0;
    int ret = 0;

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Enter, connect_agm_audio_intf_to_stream\n");

    control = connect ? "connect" : "disconnect";

    ctl_len = snprintf(NULL, 0, "%s%d %s", stream, device, control) + 1;
    mixer_str = calloc(1, ctl_len);
    if (!mixer_str) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "mixer_str calloc failed\n");
        return -ENOMEM;
    }
    snprintf(mixer_str, ctl_len, "%s%d %s", stream, device, control);

    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Invalid mixer control: %s\n", mixer_str);
        free(mixer_str);
        return -ENOENT;
    }

    ret = mixer_ctl_set_enum_by_string(ctl, intf_name);
    free(mixer_str);

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Exit, connect_agm_audio_intf_to_stream\n");
    return ret;
}

static int set_agm_device_media_config(struct mixer *mixer, char *intf_name,
        struct device_config *config)
{
    char *control = "rate ch fmt";
    char *mixer_str;
    struct mixer_ctl *ctl;
    long media_config[4];
    int ctl_len = 0;
    int ret = 0;

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Enter, set_agm_device_media_config\n");

    ctl_len = strlen(intf_name) + 1 + strlen(control) + 1;
    mixer_str = calloc(1, ctl_len);
    if (!mixer_str) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "mixer_str calloc failed\n");
        return -ENOMEM;
    }
    snprintf(mixer_str, ctl_len, "%s %s", intf_name, control);

    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "mixer_get_ctl_by_name failed\n");
        free(mixer_str);
        return -ENOENT;
    }

    media_config[0] = config->rate;
    media_config[1] = config->ch;
    media_config[2] = SNDRV_PCM_FORMAT_S16_LE;
    media_config[3] = AGM_DATA_FORMAT_FIXED_POINT;

    ret = mixer_ctl_set_array(ctl, &media_config, sizeof(media_config)/sizeof(media_config[0]));
    free(mixer_str);
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Exit, set_agm_device_media_config\n");
    return ret;
}

static struct pcm *s_pcm = NULL;
static struct mixer *s_mixer = NULL;

void device_sg_deinit(void)
{
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Enter, device_sg_deinit\n");
    if (s_pcm) {
        pcm_close(s_pcm);
        s_pcm = NULL;
    }
    if (s_mixer) {
        mixer_close(s_mixer);
        s_mixer = NULL;
    }
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Exit, device_sg_deinit\n");
}

void device_sg_init(int dev, unsigned int dkv)
{
    struct pcm_config config;
    char *intf_name = "PCM_RT_PROXY-RX-2";
    struct device_config *dev_config = NULL;
    int card = 100, device = dev;
    unsigned int stream_kv = 0;
    unsigned int device_kv = dkv;

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE,
        "Enter, device_sg_init. card: %d, device: %d, device_kv: %x\n", card, device, device_kv);

    if (s_mixer || s_pcm) {
        AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE,
            "device_sg_init: already initialized, skipping");
        return;
    }

    memset(&config, 0, sizeof(config));
    config.channels = 2;
    config.rate = 48000;
    config.period_size = 1024;
    config.period_count = 4;

    dev_config = malloc(sizeof(struct device_config));
    if (!dev_config) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to allocate memory for dev config.");
        return;
    }
    strlcpy(dev_config->name, intf_name, sizeof(dev_config->name));
    dev_config->bits = 16;
    dev_config->rate = config.rate;
    dev_config->ch = config.channels;
    dev_config->format = config.format;

    s_mixer = mixer_open(card);
    if (!s_mixer) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to open mixer\n");
        free(dev_config);
        return;
    }
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "mixer_open() successful.\n");

    if (set_agm_device_media_config(s_mixer, intf_name, dev_config)) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to set device media config\n");
        goto err_close_mixer;
    }

    if (set_agm_audio_intf_metadata(s_mixer, intf_name, device_kv,
                                    dev_config->rate, dev_config->bits, stream_kv)) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to set device metadata\n");
        goto err_close_mixer;
    }

    if (connect_agm_audio_intf_to_stream(s_mixer, device, intf_name, true)) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to connect pcm to audio interface\n");
        goto err_close_mixer;
    }

    s_pcm = pcm_open(card, device, PCM_OUT, &config);
    if (!s_pcm || !pcm_is_ready(s_pcm)) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Unable to open PCM device %u (%s)\n",
                device, pcm_get_error(s_pcm));
        s_pcm = NULL;
        goto err_disconnect_and_close;
    }
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "pcm_open() successful.\n");

    if (pcm_start(s_pcm) < 0) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "pcm_start error\n");
        pcm_close(s_pcm);
        s_pcm = NULL;
        goto err_disconnect_and_close;
    }

    free(dev_config);
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Exit, device_sg_init successful.");
    return;

err_disconnect_and_close:
    connect_agm_audio_intf_to_stream(s_mixer, device, intf_name, false);
err_close_mixer:
    free(dev_config);
    mixer_close(s_mixer);
    s_mixer = NULL;
}
