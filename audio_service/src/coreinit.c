/*=========================================================================

  @file coreinit.c

  @brief Implements the MSM family/asic specific initilization for
  audio_service

  ==========================================================================*/

/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "glink.h"
#include "adsp_default_listener.h"
#include "coreinit.h"
#include "audio_log_util.h"
#include "gsl_intf.h" //RTGM
#include "agm_server_wrapper_dbus.h"
#include "agm/agm_api.h"
#ifdef ENABLE_AUDIO_BACKEND
#include "gsl_be.h"
#endif
extern int adm_init(const char* service_type);
extern int adm_deinit(void);
/*===Global definitions======*/
#define AUDIO_BASE ""
#define combpath(x,y) (x#y)
#define LPASS_CONFIG_PATH combpath(AUDIO_BASE, /etc/sysconfig/lpass_cfg)
#ifdef ENABLE_MDF
#define GPDSP0_CONFIG_PATH  combpath(AUDIO_BASE, /etc/sysconfig/gpdsp0_cfg)
#define GPDSP1_CONFIG_PATH  combpath(AUDIO_BASE, /etc/sysconfig/gpdsp1_cfg)
#endif // ENABLE_MDF

xport_smem_init_cfg init_cfg[] = {
    { GLINK_GET_REMOTE_SS_NAME("adsp0","0","0","0"), "adsp_apps,apps_adsp,LOOPBACK_CTL_LPASS", 1},
#ifdef ENABLE_MDF
    { GLINK_GET_REMOTE_SS_NAME("adsp1","0","0","0"), "adsp1_apps,apps_adsp1", 1 },
    { GLINK_GET_REMOTE_SS_NAME("adsp2","0","0","0"), "adsp2_apps,apps_adsp2", 1 },
#endif
};


/* @brief : Initializes all the core components before the audio driver can
 *          be loaded into the process
 *
 * @brief This function initializes
 *  1. glink; 2. shared mem w/ dsp; 3. agm dbus ipc;
 *  4. mclk; 5. HW codec device config;
 *
 * @param[in] flags - flags to be passed to the audio service
 * @param[in] lpass_config_path - path to the lpass config file
 *
 * @return int - 0 on success, -1 on failure
 */
int audio_service_base_init(int flags, const char *lpass_config_path)
{
    pthread_attr_t attr;
    pthread_t cdc_tid;
    pthread_t fastrpc_be_tid;
    pthread_t audio_postinit_tid;
    int32_t rc = 0;

//1 glink
    (void)glink_init();
    AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AUDIO_SERVICE, "glink_init done.");
//2 shmem
    (void)xport_smem_init(init_cfg, sizeof(init_cfg)/sizeof(init_cfg[0]));
    AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AUDIO_SERVICE, "xport_smem_init done.");
//3 agm dbus ipc
    rc = ipc_agm_init();
    if (rc != 0) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "AGM init failed.");
        goto exit;
    } else {
        AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AUDIO_SERVICE, "ipc_agm_init succeeded.");
    }
//4 mclk
    rc = ar_prm_set_default_mclk();
    if (rc != 0) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "ar_prm_set_default_mclk failed.");
    }
//4.5 sleep 5 for successful i2c read/write.
    sleep(5);
//5 ar_dev_init
    rc = ar_dev_init();
    if (rc != 0) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "ar_dev_init failed.");
        goto exit;
    } else {
        AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AUDIO_SERVICE, "ar_dev_init succeeded.");
    }
//6 Initialize Virtio Audio Device's Audio Device Manager
    (void)adm_init("ar");
    AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AUDIO_SERVICE, "adm_init() done for Virtio Audio Device");
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Pre-init complete");

exit:
    return 0;
}

int audio_service_base_deinit(void)
{
    int32_t rc = 0;
    (void)adm_deinit();
    (void)ipc_agm_deinit();
    if (rc) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "failure in audio_service_base_deinit");
    }

    return rc;
}