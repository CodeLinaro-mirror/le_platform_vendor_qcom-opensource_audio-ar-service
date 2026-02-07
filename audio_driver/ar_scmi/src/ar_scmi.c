/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include "ar_scmi.h"
#include "ar_osal_error.h"

int ar_scmi_fd = -1;

#define AR_LOGE(...) AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AR_SCMI, __VA_ARGS__)
#define AR_LOGI(...) AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AR_SCMI, __VA_ARGS__)
#define AR_LOGD(...) AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AR_SCMI, __VA_ARGS__)

uint32_t ar_scmi_dev_open(void)
{
    ar_scmi_fd = open(AR_SCMI_NODE, O_RDWR);
    if (ar_scmi_fd < 0)
    {
        AR_LOGE("Failed to open Audio SCMI node: %s", AR_SCMI_NODE);
        return AR_EFAILED;
    }

    AR_LOGE("AR SCMI node: %s opened successfully with fd :%d", AR_SCMI_NODE, ar_scmi_fd);
    return AR_EOK;
}

#ifdef __QNX__
uint32_t ar_scmi_reset_ioctl(scmi_rst_oper_t oper)
{
    int rc = AR_EOK;

    int32_t ret_call = -1;
    scmi_oper_devctl_t ar_scmi_req = { 0 };

    ar_scmi_req.domain_id = 0;
    ar_scmi_req.oper = oper;

    rc = devctl(ar_scmi_fd, DCMD_SCMI_RST, &ar_scmi_req, sizeof(scmi_oper_devctl_t), &ret_call);
    if (rc != 0)
    {
        AR_LOGE("AR SCMI ioctl failed: %s with ret_call: %u rc: %d", AR_SCMI_NODE, ret_call, rc);
        return AR_EFAILED;
    }

    AR_LOGE("AR SCMI Reset ioctl operation: %u is successfull for: %s with fd:%d", oper, AR_SCMI_NODE, ar_scmi_fd);
    return AR_EOK;
}
#else
uint32_t ar_scmi_reset_ioctl(scmi_rst_oper_t oper)
{
    int rc = AR_EOK;

    scmi_oper_ioctl_t ar_scmi_req = { 0 };

    ar_scmi_req.proto = SCMI_PROTO_RESET;
    ar_scmi_req.oper = oper;
    (void)strlcpy(ar_scmi_req.name, AR_RESET_DOMAIN, NAME_LEN);

    rc = ioctl(ar_scmi_fd, SCMI_IOCTL_RST, &ar_scmi_req);
    if (rc < 0)
    {
        AR_LOGE("AR SCMI ioctl failed: %s with rc: %d", AR_SCMI_NODE, rc);
        return AR_EFAILED;
    }

    AR_LOGE("AR SCMI Reset ioctl operation: %u is successfull for: %s with fd:%d", oper, AR_SCMI_NODE, ar_scmi_fd);
    return AR_EOK;
}
#endif

uint32_t ar_scmi_dev_close(void)
{
    if (ar_scmi_fd < 0)
    {
        AR_LOGE("AR SCMI node: %s not opened", AR_SCMI_NODE);
        return AR_EHANDLE;
    }

    close(ar_scmi_fd);
    return AR_EOK;
}
