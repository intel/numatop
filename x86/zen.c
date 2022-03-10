/*
 * Copyright (c) 2023, AMD Corporation
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

/* This file contains the Zen platform specific functions. */

#include <inttypes.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include "../common/include/os/linux/perf_event.h"
#include "../common/include/os/plat.h"
#include "include/zen.h"

#define IBS_OP_PMU_TYPE_PATH \
	"/sys/bus/event_source/devices/ibs_op/type"

static plat_event_config_t s_zen_config[PERF_COUNT_NUM] = {
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, 0, 0, 0, 0, "LsNotHaltedCyc" },
	{ PERF_TYPE_RAW, 0x0000000000004043, 0, 0, 0, 0, "LsDmndFillsFromSys.DRAM_IO_Far" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, 0, 0, 0, 0, "LsNotHaltedCyc" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, 0x53, 0, 0, 0, "ExRetOps" },
	{ PERF_TYPE_RAW, 0x0000000000000843, 0, 0, 0, 0, "LsDmndFillsFromSys.DRAM_IO_Near" },
};

/*
 * Owing to the nature of IBS uop tagging, a higher sampling period is
 * required to capture meaningful samples. All samples may not originate
 * from a memory access instruction and require additional filtering.
 */
static plat_event_config_t s_zen_ll = {
	0, 0x0000000000000000, 0, 0, LL_THRESH * 10, 0, "IbsOpCntCycles"
};

void
zen_profiling_config(perf_count_id_t perf_count_id, plat_event_config_t *cfg)
{
	plat_config_get(perf_count_id, cfg, s_zen_config);
}

static int
zen_ibs_op_pmu_type(void)
{
	int fd, type, i;
	char buf[32];

	if ((fd = open(IBS_OP_PMU_TYPE_PATH, O_RDONLY)) < 0)
		return (-1);

	if ((i = read(fd, buf, sizeof (buf) - 1)) <= 0) {
		close(fd);
		return (-1);
	}

	close(fd);
	buf[i] = 0;
	if ((type = atoi(buf)) == 0)
		return (-1);

	return (type);
}

void
zen_ll_config(plat_event_config_t *cfg)
{
	memcpy(cfg, &s_zen_ll, sizeof (plat_event_config_t));
	cfg->type = zen_ibs_op_pmu_type();
}

int
zen_offcore_num(void)
{
	return (2);
}
