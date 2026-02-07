/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <dlfcn.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#include "aud_status.h"
#include "audio_dac.h"
#include "audio_expander.h"
#include "audio_adc.h"
#include "audio_log_util.h"
#include "ar_osal_error.h"
#include "dac_status.h"
#include "adc_status.h"
#include "ar_scmi.h"
#ifndef __QNX__
#include <uapi/misc/qcom_uscmi.h>
#endif

/******************************************************************************/
/*********************************** MACROS ***********************************/
/******************************************************************************/
#define AR_LOGE(...) AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AR_DEV_PLUGIN, __VA_ARGS__)
#define AR_LOGI(...) AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AR_DEV_PLUGIN, __VA_ARGS__)
#define AR_LOGD(...) AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AR_DEV_PLUGIN, __VA_ARGS__)

/******************************************************************************/
/******************************* GLOBAL VARIABLES *****************************/
/******************************************************************************/
uint32_t expander_handle = 0;
uint32_t dac_handle = 0;

/******************************************************************************/
/************************ INTERNAL FUNCTION DEFINITION ************************/
/******************************************************************************/
/**
 * \fn int32_t ar_dev_init()
 * \brief This function inits the ar dev plugin
 * \return int32_t
 */
int32_t ar_dev_init(void)
{
    uint32_t rc = 0;
    int32_t step = 0;
    audio_expander_init_config_t expander_init_cfg = {
        0,
    };
    audio_dac_init_config_t dac_init_cfg = {
        0,
    };
    dac_enable_t dac_enable = {
        0,
    };
    fmt_payload_t dac_format = {
        0,
    };
    audio_adc_init_config_t adc_init_cfg = {
        0,
    };

    pthread_setname_np(pthread_self(), "ar_dev_init");

    step = 1;
    AR_LOGI("opening ar scmi device node");
    rc = ar_scmi_dev_open();
    if (rc) {
       AR_LOGE("ar_scmi_dev_open failed");
       goto exit;
    }

    step = 2;
    rc = ar_scmi_reset_ioctl(SCMI_RST_ASSERT);
    if (rc) {
       AR_LOGE("ar_scmi_reset_ioctl failed for operation:%u", SCMI_RST_ASSERT);
       goto exit;
    }
    AR_LOGI("ar scmi gpio assert operation done");

    step = 3;
    rc = ar_scmi_reset_ioctl(SCMI_RST_DEASSERT);
    if (rc) {
       AR_LOGE("ar_scmi_reset_ioctl failed for operation:%u", SCMI_RST_DEASSERT);
       goto exit;
    }
    AR_LOGI("ar scmi gpio deassert operation done");

    AR_LOGI("starting audio expander and dac init");
    expander_init_cfg.i2c_device = I2C_DEVICE;
    expander_init_cfg.i2c_addr = I2C_EXPANDERS_ADDR;

    step = 4;
    rc = audio_expander_init(&expander_init_cfg);
    if (AUD_EOK != rc)
        goto exit;

    step = 5;
    rc = audio_expander_open(&expander_handle);
    if (AUD_EOK != rc)
        goto exit;

    step = 6;
    dac_enable.enable = true;
    rc = audio_expander_ioctl(expander_handle, CMD_SET_DAC_ENABLE, &dac_enable, sizeof(dac_enable));
    if (AUD_EOK != rc)
        goto exit;

    step = 7;
    rc = audio_expander_ioctl(expander_handle, CMD_SET_A2B_OUT_RESET, NULL, 0);
    if (AUD_EOK != rc)
        goto exit;

    AR_LOGI("audio expander operation done");

    step = 8;
    dac_init_cfg.i2c_device = I2C_DEVICE;
    dac_init_cfg.i2c_addr = I2C_AUDIO_DAC_ADDR;
    rc = audio_dac_init(&dac_init_cfg);
    if (DAC_EOK != rc)
        goto exit;

    step = 9;
    rc = audio_dac_open(&dac_handle);
    if (DAC_EOK != rc)
        goto exit;

    step = 10;
    dac_format.format = I2S_TDM;
    rc = audio_dac_ioctl(dac_handle, CMD_SET_DAC_FMT, &dac_format, sizeof(dac_format));
    if (DAC_EOK != rc)
        goto exit;

    AR_LOGI("audio dac operation done");

    step = 11;
    adc_init_cfg.i2c_device = I2C_DEVICE;
    adc_init_cfg.i2c_addr = I2C_AUDIO_ADC_ADDR;
    rc = audio_adc_init(&adc_init_cfg);
    if (ADC_EOK != rc)
        goto exit;

    AR_LOGI("audio adc operation done");

    return AR_EOK;

exit:
    AR_LOGE("ar_dev_init failed at step %d", step);
    return rc;
}

/**
 * \fn int32_t ar_dev_dinit()
 * \brief This function deinits the ar dev plugin
 * \return int32_t
 */
int32_t ar_dev_dinit(void)
{
    uint32_t rc = 0;

    rc = audio_expander_close(expander_handle);
    if (AUD_EOK != rc)
        AR_LOGE("audio expander close fail");

    rc = audio_expander_deinit();
    if (AUD_EOK != rc)
        AR_LOGE("audio expander deintialization fail");

    rc = audio_dac_close(dac_handle);
    if (DAC_EOK != rc)
        AR_LOGE("audio dac close fail");

    rc = audio_dac_deinit();
    if (DAC_EOK != rc)
        AR_LOGE("audio dac deintialization fail");

    rc = audio_adc_deinit();
    if (ADC_EOK != rc)
        AR_LOGE("audio dac deintialization fail");

    rc = ar_scmi_dev_close();
    if (AR_EOK != rc)
       AR_LOGE("ar_scmi_dev_close failed");

    return rc;
}

/**
 * \fn uint32_t ar_dev_get_dev_count(void)
 * \brief This function returns the total number of supported devices.
 * \return uint32_t
 */
uint32_t ar_dev_get_dev_count(void) { return AR_EOK; }

/**
 * \fn int32_t ar_dev_msg(uint32_t cmd, void* params, uint32_t size)
 * \brief This function handles ar dev supported features
 * \return int32_t
 */
int32_t ar_dev_msg(uint32_t cmd, void* params, uint32_t size) { return AR_EOK; }
