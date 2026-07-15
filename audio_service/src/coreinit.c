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
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "device_sg.h"
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
extern int32_t ar_dev_init(void);

static void *ar_dev_init_thread(void *arg)
{
    int32_t rc = 0;

    pthread_setname_np(pthread_self(), "ar_dev_init");

    AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AUDIO_SERVICE, "ar_dev_init starts");

    rc = ar_dev_init();

    if(rc != 0)
       AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "ar_dev_init failed with error");

    return NULL;
}
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

static uint32_t s_dev_id;
static uint32_t s_device_kv;
static bool s_device_sg_configured = false;

void audio_service_set_sg_params(uint32_t device_id, uint32_t device_kv)
{
    s_dev_id = device_id;
    s_device_kv = device_kv;
    s_device_sg_configured = true;
}

/*
 * audio_service_ssr_cb - called by AGM when the audio subsystem goes down
 * or comes back up after a Subsystem Restart (SSR).
 *
 * On DN: tear down device subgraph and log the event.
 * On UP: restore mclk and re-initialise device subgraph.
 */
static void audio_service_ssr_cb(enum agm_ssr_event_type event_type,
                                 void *client_data)
{
    /* set when DN is received; cleared after UP recovery */
    static bool ssr_down_seen = false;

    (void)client_data;

    if (event_type == AGM_SSR_EVENT_AUDIO_SVC_DN) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE,
                       "audio-ar-service: audio subsystem DOWN (SSR)");
        ssr_down_seen = true;
        if (s_device_sg_configured) {
            device_sg_deinit();
        }
    } else if (event_type == AGM_SSR_EVENT_AUDIO_SVC_UP) {
        AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE,
                       "audio-ar-service: audio subsystem UP (SSR recovery)");
        if (ssr_down_seen) {
            ssr_down_seen = false;
            /*
             * Workaround: satellite DSP IPCC channels and firmware memory
             * subsystem re-initialize asynchronously after SSR UP and are
             * not ready immediately. Without this delay, gsl_mdf_utils_shmem_alloc
             * hits AR_EFAILED (satellite firmware not ready) or triggers an
             * IPCC NULL-deref crash on adsp2 (ipcc_irq46, BADVA=0x6c).
             * 2 s gives sufficient margin for all satellite procs to reach
             * a mappable state before GSL recovery runs.
             */
            sleep(2);
            if (ar_prm_set_default_mclk() != 0) {
                AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE,
                               "SSR: ar_prm_set_default_mclk failed");
            }
            else {
                AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE,
                               "SSR: ar_prm_set_default_mclk succeeded");
            }
            if (s_device_sg_configured) {
                device_sg_init((int)s_dev_id, s_device_kv);
                AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE,
                    "device_sg_init completed on SSR recovery");
            }
        } else {
            AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE,
                           "audio-ar-service: UP received without prior DN, skipping device_sg_init");
        }
    }
}

/* @brief : Initializes all the core components before the audio driver can
 *          be loaded into the process
 *
 * @brief This function initializes
 *  1. glink; 2. shared mem w/ dsp; 3. agm dbus ipc;
 *  3.1 AGM SSR callback; 3.2 D-Bus SSR broadcast;
 *  4. mclk; 4.1 sleep (i2c settle); 5. HW codec device config;
 *  6. Virtio Audio Device Manager; 7. device subgraph;
 *
 * @param[in] flags - flags to be passed to the audio service
 * @param[in] lpass_config_path - path to the lpass config file
 *
 * @return int - 0 on success, -1 on failure
 */
int audio_service_base_init(int flags, const char *lpass_config_path)
{
    pthread_attr_t attr;
    pthread_t ar_dev_tid;
    int32_t rc = 0;

    rc = pthread_attr_init(&attr);
    if (rc != 0) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE,
                       "pthread_attr_init failed: %d", rc);
        return rc;
    }

//1 glink
    (void)glink_init();
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "glink_init done.");
//2 shmem
    (void)xport_smem_init(init_cfg, sizeof(init_cfg)/sizeof(init_cfg[0]));
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "xport_smem_init done.");
//3 agm dbus ipc
    rc = ipc_agm_init();
    if (rc != 0) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "AGM init failed.");
        goto exit;
    }
/*
 * 3.1 Register the audio service's own SSR callback directly with AGM.
 *     audio_service_ssr_cb tears down the device subgraph on DN and
 *     restores mclk + reinitialises it on UP.  Registered before the
 *     D-Bus bridge (step 3.2) so service-internal recovery completes
 *     before clients are notified.
 */
    rc = agm_register_ssr_cb(audio_service_ssr_cb, NULL);
    if (rc != 0) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE,
                       "agm_register_ssr_cb failed rc=%d", rc);
        /* non-fatal: continue without SSR handling */
        rc = 0;
    } else {
        AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AUDIO_SERVICE,
                       "agm_register_ssr_cb succeeded.");
    }
//3.2 register D-Bus SSR broadcast so clients are notified of subsystem restart
    rc = ipc_agm_ssr_register();
    if (rc != 0) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE,
                       "ipc_agm_ssr_register failed rc=%d", rc);
        /* non-fatal: continue without SSR D-Bus broadcast */
        rc = 0;
    } else {
        AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AUDIO_SERVICE,
                       "ipc_agm_ssr_register succeeded.");
    }
//4 mclk — required before ar_dev_init; abort if MCLK cannot be configured
    rc = ar_prm_set_default_mclk();
    if (rc != 0) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "ar_prm_set_default_mclk failed.");
        goto exit;
    }
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "ar_prm_set_default_mclk succeeded.");
//5 ar_dev_init — run in thread so MCLK settling overlaps with adm_init
    if (0 != pthread_create(&ar_dev_tid, &attr, (void*)ar_dev_init_thread, NULL)) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "failure creating ar_dev_init thread");
        goto exit;
    }

//6 Initialize Virtio Audio Device's Audio Device Manager
    (void)adm_init("ar");
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "adm_init() done for Virtio Audio Device");

    // join to ensure DAC is ready before service is declared ready
    pthread_join(ar_dev_tid, NULL);
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "Pre-init complete");

//7 device subgraph init
    if (s_device_sg_configured) {
        device_sg_init((int)s_dev_id, s_device_kv);
    }

    pthread_setname_np(pthread_self(), "ar_audio_main");
exit:
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE,
                       "Exit from audio_service_base_init with error code %d", rc);
    }
    return rc;
}

int audio_service_base_deinit(void)
{
    int32_t rc = 0;

    if (s_device_sg_configured) {
        device_sg_deinit();
    }
    (void)adm_deinit();
    ipc_agm_ssr_deregister();
    agm_deregister_ssr_cb(audio_service_ssr_cb, NULL);
    ipc_agm_deinit();

    if (rc) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "failure in audio_service_base_deinit");
    }

    return rc;
}
