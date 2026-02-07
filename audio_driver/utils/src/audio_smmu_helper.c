/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "audio_smmu_helper.h"
#include <errno.h>

#define EOK 0

static struct audio_smmu_helper* asmmu = NULL;
static struct plat_vfio pvfio[DEVICE_NUM_MAX];

int audio_smmu_helper_init()
{
    int rc = EOK;
    int i;

    for (i = 0; i < DEVICE_NUM_MAX; i++)
    {
        rc = plat_vfio_device_init(dev_name[i], &pvfio[i]);
        if (rc < 0)
        {
            AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_ASMMU, "audio smmu vfio init failed for device : %s", dev_name[i]);
            goto exit;
        }
        AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_ASMMU,"audio smmu vfio init is successful for device : %s", dev_name[i]);
    }

exit:
    return rc;
}

audio_smmu_helper_t* audio_smmu_helper_create(void)
{
    int rc = EOK;

    asmmu = (audio_smmu_helper_t*) malloc(sizeof(audio_smmu_helper_t));
    if (asmmu == NULL)
    {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_ASMMU, "malloc for asmmu failed");
        return asmmu;
    }
    memset(asmmu, 0, sizeof(audio_smmu_helper_t));

    /* set sid to 1, for normal audio (adsp) usecases to work fine.
     * For ams usecase, this config will be overridden from csd2 ams with
     * its own smmu helper context
     */
    asmmu->sid = DEFAULT_ADSP_SID;
    asmmu->dsp_type = ADSP;

exit :
    return asmmu;
}

void* audio_smmu_helper_map(audio_smmu_helper_t* h)
{
    void* da;

    if (!h)
        return NULL;

    // Note: the result of this mapping operation should be 32-bits
    da = vfio_dma_buf_map(&pvfio[h->dsp_type], &h->smmu_ctx);
    if (h->smmu_ctx.dma_addr ==  NULL) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_ASMMU, "dma buffer mapping failed with ret = %d", da);
        da = NULL;
    } else {
        da = (void* )( h->smmu_ctx.dma_addr | ((uint64_t)(h->sid) << 32));
        AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_ASMMU, "Mapped heap fd %u to smmu ctx with DA %p", h->smmu_ctx.dmabuf_fd, da);
    }
    return da;

}

int audio_smmu_helper_unmap(audio_smmu_helper_t* h)
{
    int rc = EOK;

    if (!h)
        return -1;

    AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_ASMMU, "Unmapped heap fd %u to smmu ctx ", h->smmu_ctx.dmabuf_fd);

    rc = vfio_dma_buf_unmap(&pvfio[h->dsp_type], &h->smmu_ctx);
    if (rc)
       AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_ASMMU, "Unmapped heap fd %u to smmu ctx failed with rc = %d",
                                                     h->smmu_ctx.dmabuf_fd, rc);

    return rc;
}

void audio_smmu_helper_destroy(audio_smmu_helper_t* h)
{
    if (!h)
        return;

    free(h);
    h = NULL;
}

int audio_smmu_helper_deinit()
{
    int rc = EOK;
    int i;

    for (i = 0; i < DEVICE_NUM_MAX; i++)
    {
        rc = plat_vfio_device_deinit(&pvfio[i]);
        if (rc < 0)
        {
            AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_ASMMU,"audio smmu vfio deinit failed for device : %s", dev_name[i]);
            goto exit;
        }
        AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_ASMMU,"audio smmu vfio deinit is successful for device : %s", dev_name[i]);
    }

exit:
    return rc;
}

