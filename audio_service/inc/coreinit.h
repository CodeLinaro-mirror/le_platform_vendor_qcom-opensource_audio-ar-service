/*=========================================================================

  @file coreinit.h

  @brief API definitions for the platform/core level initilizations.

==========================================================================*/

/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef _CORE_INIT_H_
#define _CORE_INIT_H_
#include "audio_log_util.h"

/* Make a given function visible only to project*/
#define HIDDEN_ATTR __attribute__((__visibility__("hidden")))

/* @brief : Initializes all the core components before the audio driver can
 *          be loaded into the process
 *
 * @param[in] flags : for future use.
 *            lpass_config_path : LPASS configuration file location.
 *
 * @return : 0 for success, -1 otherwise
 *
 * @notes : This function sets up all the process level infrastructure
 *          required for the CSD to work. It involves the following -
 *          Initialize DAL, Slimbus, SMEM/SMD Lite, Diag, NPA
 * */
int HIDDEN_ATTR audio_service_base_init(int flags, const char *lpass_config_path);

/* @brief : De-initializes all the core components for shutdown routine.
 *
 * @return : 0 for success, -1 otherwise
 *
 * @notes : This function deinitialize the core components, including adm, mcm
 * for preparing shutdown.
 * */
int HIDDEN_ATTR audio_service_base_deinit(void);
#endif
