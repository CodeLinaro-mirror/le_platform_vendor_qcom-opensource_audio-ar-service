/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef _AR_PRM_H_
#define _AR_PRM_H_

#include "mmdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ar_prm_clk_disable {
    uint32_t num_clks;
    uint32_t clk_ids[];
};

struct ar_prm_header
{
    uint32_t miid;
    uint32_t param_id;
    uint32_t param_size;
    uint32_t error_code;
};

struct ar_clk_config_entry {
  uint32_t id;
  /**< Unique Clock ID of the clock being requested. */
  uint32_t freq;
  /**< Clock frequency in Hz to set. */
  uint32_t attri;
  /**< Divider for two clocks that are coupled, if necessary. */
  uint32_t root;
  /**< Root clock source. */
  uint32_t mux_index;
  /**< Root Clock source to use. */
  uint32_t divider_2x;
  /**< Integer divider to use (2x the desired divider). */
  uint32_t counter_m_value;
  /**< The M value for any M/N counter (0 to bypass). */
  uint32_t counter_n_value;
  /**< The N value for M/N counter. */
  uint32_t d_value;
  /**< Twice the D value for any M/N counter. */
 };

/**
  Payload of #CSD2_CMD_CLK_CONFIG
  Multiple entries are required if more than one clock configuration to be set.
 */
struct ar_clk_config {
  uint32_t num_clks;
  /**< Number of clocks to be configured. */

  struct ar_clk_config_entry entries[];
  /**< clock configuration payload entry. @newpagetable*/
};

int32_t agm_prm_set_default_mclk();
int32_t agm_prm_release_default_mclk();

// int32_t ar_prm_clk_enable_v2(struct ar_clk_config *clks);
// int32_t ar_prm_clk_enable(struct ar_clk_config *clks);

// int32_t ar_prm_clk_disable_v2(struct ar_prm_clk_disable *clks);
// int32_t ar_prm_clk_disable(struct ar_prm_clk_disable *clks);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* _AGM_PRM_H_ */