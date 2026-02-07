/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <stddef.h>
#include <time.h>
#include <unistd.h>
#include <dlfcn.h>
#include "audio_expander.h"
#include <linux/gpio.h>
#include "aud_status.h"
#include "audio_dac.h"
#include "ar_osal_error.h"
#include "audio_log_util.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdbool.h>

#define AR_LOGE(...) AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AR_DEV_PLUGIN, __VA_ARGS__)
#define AR_LOGI(...) AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AR_DEV_PLUGIN, __VA_ARGS__)
#define AR_LOGD(...) AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AR_DEV_PLUGIN, __VA_ARGS__)

#define GPIO_DEV_NAME               "/dev/gpiochip0"
static int (*merc_app_main_fn)(int argc, char **argv) = 0;
static const char merc_app_lib[] = "libaudio-nxp-auto.so";
/*
* Avoid reset of io-expander by toggling GPIO51 for Lemans
* No audio heard for playback from SOC -> DAC,
* as io-expander is getting reset
* Toggle gpio51 from LOW to HIGH , so that MUX doesnt get reset
*/
static void io_exp_mux_configure(void)
{
     const char *gpio_device_name = GPIO_DEV_NAME;
     int gpio_pin = MUX_GPIO;
     int fd = 0;
     int ret;
     struct gpiohandle_request req;
     struct gpiohandle_data data;

    fd = open(gpio_device_name, O_RDWR);
    if (fd < 0) {
        AR_LOGE( "Error: could not open device %s, error %d\n",
               gpio_device_name, fd);
        return;
    }

    AR_LOGI("gpio device %s open = [%d] \n", gpio_device_name, fd);
    req.flags = GPIOHANDLE_REQUEST_OUTPUT;
    req.lines = 1;
    req.lineoffsets[0] = gpio_pin;

    ret = ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &req);
    if (ret == -1) {
        AR_LOGE( "request line handle failure %d", ret);
        goto fail;
    };

    data.values[0] = 1;
    ret = ioctl(req.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
    if (ret == -1) {
        AR_LOGE( "set line value failure %d", ret);
        goto fail;
    }

    ret = ioctl(req.fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data);
    if (ret == -1) {
        AR_LOGE("get line value failure %d", ret);
        goto fail;
    }

    AR_LOGI(
           "toggle gpio %d output to %d\n",
           gpio_pin,
           data.values[0]);

fail:
    close(fd);

}

/*! \fn int32_t ar_dev_init()
  \brief This function inits the ar dev plugin
  \return int32_t
  */

int32_t ar_dev_init(void)
{
    void * handle = NULL;
    uint32_t rc = 0;
    uint32_t io_handle = 0;
    uint32_t dac_handle = 0;
    int32_t step = 0;
    audio_expander_init_config_t init_cfg = {0,};
    audio_dac_init_config_t dac_init_cfg = {0,};
    dac_enable_t enable_payload_t = {0,};
    switch_enable_t switch_payload_t = {0, };
    switch_source_payload_t switch_source_t = {0, };
    fmt_payload_t fmt_t = {0, };

    pthread_setname_np(pthread_self(), "ar_dev_init");

    AR_LOGI("Toggle GPIO 51 HIGH to avoid reset of MUX");
    io_exp_mux_configure();

    AR_LOGI("starting audio expander and dac init");
    init_cfg.i2c_device = I2C_DEVICE; /*/dev/i2c11 for QUP21 required*/
    init_cfg.i2c_addr = I2C_EXPANDERS_ADDR;

    step = 1;
    rc = audio_expander_init(&init_cfg);
    rc |= audio_expander_open(&io_handle);
    if (AUD_EOK != rc)
        goto exit;

    step = 2;
    enable_payload_t.enable = true;
    rc = audio_expander_ioctl(io_handle, CMD_SET_DAC_ENABLE,
            &enable_payload_t, sizeof(dac_enable_t));
    if (AUD_EOK != rc)
        goto exit;

    step = 3;
    switch_payload_t.enable = true;
    rc = audio_expander_ioctl(io_handle, CMD_SET_SWITCH_ENABLE,
            &switch_payload_t, sizeof(switch_enable_t));
    if (AUD_EOK != rc)
        goto exit;

    step = 4;
    switch_source_t.src = MERCURY_AUDIO;
    AR_LOGI("TDM source 0x%0x",switch_source_t.src);
    rc = audio_expander_ioctl(io_handle, CMD_SET_SWITCH_SOURCE,
            &switch_source_t, sizeof(switch_source_t));
    if (AUD_EOK != rc)
        goto exit;

    step = 5;
    rc = audio_expander_ioctl(io_handle, CMD_SET_A2B_OUT_RESET,
            &switch_source_t, sizeof(switch_source_t));
    if (AUD_EOK != rc)
        goto exit;

    AR_LOGI("audio expander intialization success\n");
    audio_expander_close(io_handle);
    audio_expander_deinit();

    step = 6;
    AR_LOGI("Executing Mercury App");
    usleep(500*1000);//sleep 500ms to otherwise will have frame_data_sync error
    handle = dlopen(merc_app_lib, RTLD_NOW);
    if (handle == NULL) {
        AR_LOGE("Cannot open %s", merc_app_lib);
        return AUD_EFAILED;
    }

    merc_app_main_fn = dlsym(handle, "main");
    if (merc_app_main_fn == NULL) {
        AR_LOGE("cannot retrieve symbol main");
        dlclose(handle);
        return AUD_EFAILED;
    }
    if (merc_app_main_fn(0, NULL)) {
        AR_LOGE("Mercury App main function failed");
        dlclose(handle);
        return AUD_EFAILED;
    }

    step = 7;

    dac_init_cfg.i2c_device = I2C_DEVICE;
    dac_init_cfg.i2c_addr = I2C_AUDIO_DAC_ADDR;
    rc = audio_dac_init(&dac_init_cfg);
    rc |= audio_dac_open(&dac_handle);
    if (AUD_EOK != rc)
        goto exit;

    step = 8;
    fmt_t.format = I2S_TDM;
    rc = audio_dac_ioctl(dac_handle, 0x1001,
            &fmt_t, sizeof(fmt_payload_t));
    if (AUD_EOK != rc)
        goto exit;
    AR_LOGI("audio_expander_dac intialization success");
    audio_dac_deinit();

    return AUD_EOK;

exit:
    AR_LOGI("audio_expander_dac_init failed at step %d", step);
    return rc;
}

/*! \fn int32_t ar_dev_dinit()
  \brief This function deinits the ar dev plugin
  \return int32_t
  */
int32_t ar_dev_dinit(void)
{
    return 0;
}

/*! \fn uint32_t ar_dev_get_dev_count(void)
  \brief This function returns the total number of supported devices.
  \return uint32_t
  */
uint32_t ar_dev_get_dev_count(void)
{
    return 0;
}

/*! \fn int32_t ar_dev_msg()
  \brief This function handles ar dev supported features
  \return int32_t
  */
int32_t ar_dev_msg(uint32_t cmd, void* params, uint32_t size)
{
    return 0;
}

