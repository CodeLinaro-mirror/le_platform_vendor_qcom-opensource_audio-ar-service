/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef _AR_SCMI_H_
#define _AR_SCMI_H_

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "audio_log_util.h"
#ifdef __QNX__
#include "scmi_devctl.h"
#else
#include <uapi/misc/qcom_uscmi.h>
#endif

#ifdef __QNX__
#define AR_SCMI_NODE "/dev/scmi/audio"
#else
#define AR_SCMI_NODE "/dev/scmi_audio"
#endif
#define AR_RESET_DOMAIN "reset_audio"


#ifdef __cplusplus
extern "C" {
#endif

uint32_t ar_scmi_dev_open();
uint32_t ar_scmi_reset_ioctl(scmi_rst_oper_t oper);
uint32_t ar_scmi_dev_close();

#ifdef __cplusplus
}
#endif


#endif //_AR_SCMI_H_
