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

#ifndef _NUMATOP_PLAT_H
#define	_NUMATOP_PLAT_H

#include <sys/types.h>
#include <inttypes.h>
#include "../types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLAT_EVENT_DESC_SIZE	64

typedef struct _plat_event_config {
	uint32_t type;
	/*
	 * config = "code + umask" if type is PERF_TYPE_RAW or
	 * event_id if type is PERF_TYPE_HARDWARE.
	 */
	uint64_t config;
	uint64_t other_attr;
	uint64_t extra_value;
	char desc[PLAT_EVENT_DESC_SIZE];
} plat_event_config_t;

extern uint64_t g_sample_period[COUNT_NUM][PRECISE_NUM];

typedef void (*pfn_plat_profiling_config_t)(count_id_t,
    plat_event_config_t *);
typedef void (*pfn_plat_ll_config_t)(plat_event_config_t *);
typedef int (*pfn_plat_offcore_num_t)(void);

extern int plat_detect(void);
extern void plat_profiling_config(count_id_t, plat_event_config_t *);
extern void plat_ll_config(plat_event_config_t *);
extern void plat_config_get(count_id_t, plat_event_config_t *, plat_event_config_t *);
extern int plat_offcore_num(void);

#ifdef __cplusplus
}
#endif

#endif /* _NUMATOP_PLAT_H */
