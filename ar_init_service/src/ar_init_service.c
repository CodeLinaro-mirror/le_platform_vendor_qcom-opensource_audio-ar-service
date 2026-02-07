/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <stdlib.h>
#include <stdint.h>
#include <tinyalsa/asoundlib.h>
#include <sound/asound.h>
#include <agm/agm_api.h>
#include "audio_log_util.h"

#define SAMPLINGRATE 0xA5000000   /**< @h2xmle_name{SamplingRate} */
#define BITWIDTH 0xA6000000   /**< @h2xmle_name{BitWidth} */
#define GAIN 0xD2000000   /**< @h2xmle_name{Gain} */
#define DEVICERX 0xA2000000   /**< @h2xmle_name{DeviceRX} */

static struct device_config {
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
        if (ckv)
            free(ckv);
        if (gkv)
            free(gkv);
        if (prop)
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

    prop->prop_id = 0;  //Update prop_id here
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
        return ENOENT;
    }

    ret = mixer_ctl_set_array(ctl, metadata, sizeof(num_gkv) + sizeof(num_ckv) + gkv_size + ckv_size + prop_size);

    free(gkv);
    free(ckv);
    free(prop);
    free(metadata);
    free(mixer_str);

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Exit, set_agm_audio_intf_metadata\n");
    return ret;
}

static int connect_agm_audio_intf_to_stream(struct mixer *mixer, unsigned int device, char *intf_name, bool connect)
{
    char *stream = "PCM";
    char *control;
    char *mixer_str;
    struct mixer_ctl *ctl;
    int ctl_len = 0;
    int ret = 0;

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Enter, connect_agm_audio_intf_to_stream\n");

    if (connect)
        control = "connect";
    else
        control = "disconnect";

    ctl_len = strlen(stream) + 4 + strlen(control) + 1;
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
        return ENOENT;
    }

    ret = mixer_ctl_set_enum_by_string(ctl, intf_name);
    free(mixer_str);

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Exit, connect_agm_audio_intf_to_stream\n");
    return ret;
}

static int set_agm_device_media_config(struct mixer *mixer, char *intf_name, struct device_config *config)
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
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "mixer_str %s , ctl_len %s", intf_name, control);

    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "mixer_get_ctl_by_name failed\n");
        free(mixer_str);
        return ENOENT;
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

void device_sg_init(int dev, unsigned int dkv)
{
    struct pcm_config config;
    struct pcm *pcm = NULL;
    struct mixer *mixer = NULL;
    char *buffer, *intf_name = "PCM_RT_PROXY-RX-2";
    int playback_path = 0;//PLAYBACK;
    int size, ret = 0;
    struct device_config *dev_config = NULL;
    int card = 100, device = dev;
    unsigned int stream_kv = 0;
    unsigned int device_kv = dkv;

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Enter, device_sg_init. card: %d, device: %d, device_kv: %x\n", card, device, device_kv);

    memset(&config, 0, sizeof(config));
    config.channels = 2;
    config.rate = 48000;
    config.period_size = 1024;
    config.period_count = 4;
    config.format = 0;
    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;

    dev_config = (struct device_config *) malloc(sizeof(struct device_config));
    if (!dev_config) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to allocate memory for dev config.");
        return;
    }
    strlcpy(dev_config->name, intf_name, sizeof(dev_config->name));
    dev_config->bits = 16;
    dev_config->rate = config.rate;
    dev_config->ch = config.channels;
    dev_config->format = config.format;

    mixer = mixer_open(card);

    if (!mixer) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to open mixer\n");
        return;
    }
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "mixer_open() successful.\n");

    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE,
        "\nBackend %s dev_config rate ch bit fmt : %d, %d, %d, %d\n", intf_name,
        dev_config->rate, dev_config->ch, dev_config->bits, dev_config->format);

    /* set device/audio_intf media config mixer control */
    if (set_agm_device_media_config(mixer, intf_name, dev_config)) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to set device media config\n");
        goto err_close_mixer;
    }

    /* set audio interface metadata mixer control */
    if (set_agm_audio_intf_metadata(mixer, intf_name, device_kv,
                                dev_config->rate, dev_config->bits, stream_kv)) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to set device metadata\n");
        goto err_close_mixer;
    }

    /* connect pcm stream to audio intf */
    if (connect_agm_audio_intf_to_stream(mixer, device, intf_name, true)) { //STREAM_PCM
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Failed to connect pcm to audio interface\n");
        goto err_close_mixer;
    }

    /* open pcm device */
    pcm = pcm_open(card, device, PCM_OUT, &config);
    if (!pcm || !pcm_is_ready(pcm)) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Unable to open PCM device %u (%s)\n",
                device, pcm_get_error(pcm));
        goto err_close_mixer;
    }
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "pcm_open() successful.\n");

    size = pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));
    buffer = malloc(size);
    if (!buffer) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "Unable to allocate %d bytes\n", size);
        goto err_close_pcm;
    }

    if (pcm_start(pcm) < 0) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "start error\n");
        goto err_close_pcm;
    }
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "pcm_start() successful.\n");
    goto end;

err_close_pcm:
    pcm_close(pcm);
    mixer_close(mixer);

err_close_mixer:
    if (dev_config)
        free(dev_config);

    mixer_close(mixer);
    return;
end:
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Exit, device_sg_init successful. :-)");
    return;
}

void print_usage(const char *prog) {
    printf("Usage: %s <device> <dkv>\nExample: %s 102 0xA2000009\n", prog, prog);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    int device = atoi(argv[1]);                      // First argument: device
    unsigned int dkv = (unsigned int)strtoul(argv[2], NULL, 16);   // Second argument: dkv (supports hex)

    AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AUDIO_SERVICE, "Initializing Device Subgraph with pcm device id %d, dkv 0x%X\n", device, dkv);
    device_sg_init(device, dkv);

    AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AUDIO_SERVICE, "Device Subgraph Initialization complete.\n");
    return EXIT_SUCCESS;
}
