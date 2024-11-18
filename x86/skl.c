/*
 * Copyright (c) 2013, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* This file contains the bdw platform specific functions. */

#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include "../common/include/os/linux/perf_event.h"
#include "../common/include/os/plat.h"
#include "include/skl.h"

static plat_event_config_t s_skl_config[PERF_COUNT_NUM] = {
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, 0x53, 0, 0, 0, "cpu_clk_unhalted.core" },
	{ PERF_TYPE_RAW, 0x01B7, 0x53, 0x638000001, 0, 0, "off_core_response_0" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_REF_CPU_CYCLES, 0x53, 0, 0, 0, "cpu_clk_unhalted.ref" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, 0x53, 0, 0, 0, "instr_retired.any" },
	{ PERF_TYPE_RAW, 0x01BB, 0x53, 0x1f84000001, 0, 0, "off_core_response_1" }
};

static plat_event_config_t s_icx_config[PERF_COUNT_NUM] = {
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, 0x53, 0, 0, 0, "cpu_clk_unhalted.core" },
	{ PERF_TYPE_RAW, 0x01B7, 0x53, 0x730000001, 0, 0, "off_core_response_0" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_REF_CPU_CYCLES, 0x53, 0, 0, 0, "cpu_clk_unhalted.ref" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, 0x53, 0, 0, 0, "instr_retired.any" },
	{ PERF_TYPE_RAW, 0x01BB, 0x53, 0x104000001, 0, 0, "off_core_response_1" }
};

static plat_event_config_t s_spr_config[PERF_COUNT_NUM] = {
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, 0x53, 0, 0, 0, "cpu_clk_unhalted.core" },
	{ PERF_TYPE_RAW, 0x012A, 0x53, 0x730000001, 0, 0, "off_core_response_0" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_REF_CPU_CYCLES, 0x53, 0, 0, 0, "cpu_clk_unhalted.ref" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, 0x53, 0, 0, 0, "instr_retired.any" },
	{ PERF_TYPE_RAW, 0x012B, 0x53, 0x104000001, 0, 0, "off_core_response_1" }
};

static plat_event_config_t s_skl_ll = {
	PERF_TYPE_RAW, 0x01CD, 0x53, LL_THRESH, 0, 1, "mem_trans_retired.latency_above_threshold"
};

void
skl_profiling_config(perf_count_id_t perf_count_id, plat_event_config_t *cfg)
{
	plat_config_get(perf_count_id, cfg, s_skl_config);
}

void
icx_profiling_config(perf_count_id_t perf_count_id, plat_event_config_t *cfg)
{
	plat_config_get(perf_count_id, cfg, s_icx_config);
}

void
spr_profiling_config(perf_count_id_t perf_count_id, plat_event_config_t *cfg)
{
	plat_config_get(perf_count_id, cfg, s_spr_config);
}

void
skl_ll_config(plat_event_config_t *cfg)
{
	memcpy(cfg, &s_skl_ll, sizeof (plat_event_config_t));
}

void
icx_ll_config(plat_event_config_t *cfg)
{
	skl_ll_config(cfg);
}

void
spr_ll_config(plat_event_config_t *cfg)
{
	skl_ll_config(cfg);
}

int
skl_offcore_num(void)
{
	return (2);
}

int
icx_offcore_num(void)
{
	return skl_offcore_num();
}

int
spr_offcore_num(void)
{
	return skl_offcore_num();
}