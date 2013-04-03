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

/* This file contains the Nehalem platform specific functions. */

#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include "../common/include/linux/perf_event.h"
#include "../common/include/types.h"
#include "../common/include/plat.h"
#include "include/nhm.h"

static plat_event_config_t s_nhm_profiling[COUNT_NUM] = {
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, 0x53, 0, "cpu_clk_unhalted.core" },
	{ PERF_TYPE_RAW, 0x01B7, 0x53, 0x3011, "off_core_response_0" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_REF_CPU_CYCLES, 0x53, 0, "cpu_clk_unhalted.ref" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, 0x53, 0, "instr_retired.any" },
	{ PERF_TYPE_RAW, INVALID_CODE_UMASK, 0, 0, "off_core_response_1" }
};

static plat_event_config_t s_nhm_ll = {
	PERF_TYPE_RAW, 0x100B, 0x53, LL_THRESH, "mem_inst_retired.latency_above_threshold"
};

static void
config_get(count_id_t count_id, plat_event_config_t *cfg, plat_event_config_t *cfg_arr)
{
	cfg->type = cfg_arr[count_id].type;
	cfg->config = cfg_arr[count_id].config;
	cfg->other_attr = cfg_arr[count_id].other_attr;
	cfg->extra_value = cfg_arr[count_id].extra_value;
	strncpy(cfg->desc, cfg_arr[count_id].desc, PLAT_EVENT_DESC_SIZE);
	cfg->desc[PLAT_EVENT_DESC_SIZE - 1] = 0;
}

void
nhmex_profiling_config(count_id_t count_id, plat_event_config_t *cfg)
{
	config_get(count_id, cfg, s_nhm_profiling);
}

void
nhmep_profiling_config(count_id_t count_id, plat_event_config_t *cfg)
{
	config_get(count_id, cfg, s_nhm_profiling);
}

void
nhmex_ll_config(plat_event_config_t *cfg)
{
	memcpy(cfg, &s_nhm_ll, sizeof (plat_event_config_t));
}

void
nhmep_ll_config(plat_event_config_t *cfg)
{
	memcpy(cfg, &s_nhm_ll, sizeof (plat_event_config_t));
}

int
nhm_offcore_num(void)
{
	return (1);	
}
