/*=========================================================================

  @file main.c

  @brief Implements the main function for the audio service.

  ==========================================================================*/
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include "coreinit.h"
#include <systemd/sd-daemon.h>
#include <bootkpi/logging.h>
#include <glib.h>

#ifdef ARE_ON_APPS
#include <posal.h>
#include <spf_main.h>
#include <gpr_api.h>
#endif

#define TEST_BMETRICS_LOG(tag) bootkpi_log_line(tag)

GMainLoop *mainloop = NULL;

int audio_service_initFlags = 0;

static void print_usage(void);
int audio_service_core_init(int flags, const char *lpass_config_path);

/* handle the shutdown signal */
static void signal_handler(int sig_no)
{
    int rc = 0;
    fprintf(stderr,"audio_service : signal %d exit\n", sig_no);
    rc = audio_service_base_deinit();
    if (rc) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AUDIO_SERVICE, "audio_service_base_deinit failed, rc=%d", rc);
    }
    exit(0);
}

static void print_usage(void)
{
    printf("Usage Details:\n");
    printf("-v , Enable verbose sloging. Disabled by default. Verbosity level increases on specifying extra v.\n");
    printf("-h , Print this usage message.\n");
    printf("-f , Set LPASS config path.\n");
    fflush(stdout);
}


int audio_service_core_init(int flags, const char *lpass_config_path)
{
    static int init_done = 0;

    // only do this once.
    if (init_done)
        return 0;
    init_done = 1;

    bootkpi_log_init();
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "audio_service_base_init");
    TEST_BMETRICS_LOG("audio_service_base_init");
    // audio service base initializations
    if (0 != audio_service_base_init(flags, lpass_config_path)) {
        return -1;
    }
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "audio_service_base_init done");

    return 0;
}

int main(int argc, char** argv)
{
    int c = 0;
    int v = 0;
    int rc = 0;
    char *lpass_config_path = NULL;

    struct sigaction sig_action;
    sig_action.sa_handler = signal_handler;
    sigaction(SIGTERM, &sig_action, NULL);
    sigaction(SIGINT, &sig_action, NULL);
    sigaction(SIGQUIT, &sig_action, NULL);
    sigaction(SIGKILL, &sig_action, NULL);
    sigaction(SIGABRT, &sig_action, NULL);

    while((c = getopt( argc, argv,"vhf:")) != -1) {
        switch( c ) {
            case 'v':
                v++;
                break;
            case 'h':
                print_usage();
                if (lpass_config_path) {
                    free(lpass_config_path); // free resource before exit
                }
                return EXIT_SUCCESS;
                break;
            case 'f':
                if (lpass_config_path) {
                    free(lpass_config_path); // free resource before create new string
                }
                lpass_config_path = strdup(optarg);
                break;
            case '?':
            default :
                break;
        }
    }
    mainloop = g_main_loop_new(NULL, false);
    if (0 != audio_service_core_init(audio_service_initFlags, lpass_config_path)) {
        AUDIO_BASE_LOG(BASE_LOG_CRITICAL, MODULE_AUDIO_SERVICE, "audio_service: audio_service_core_init Error");
        if (lpass_config_path) {
            free(lpass_config_path); // free resource before exit
        }
        return EXIT_FAILURE;
    }
    if (lpass_config_path) {
        free(lpass_config_path);
    }

    sd_notify(0, "READY=1");
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "audio_service Initialization Completed. Notified as READY");
    TEST_BMETRICS_LOG("audio_service initialization completed");

    g_main_loop_run(mainloop);

    //Code should never reach here
    AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AUDIO_SERVICE, "audio_service: Exiting...");

    return EXIT_SUCCESS;
}
