/*
 * Copyright (c) 2017, IBM Corporation
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

#ifndef _NUMATOP_UI_PERF_MAP_H
#define _NUMATOP_UI_PERF_MAP_H

#include "types.h"

/*
 * Hardcoding 2 here because there is only one use case
 * for powerpc and it uses 2 perf events to get RMA. If
 * there is a case in future where more than 2 events are
 * needed, this has to be changed.
 */
#define UI_PERF_MAP_MAX		2

typedef struct _ui_perf_count_map_t {
	ui_count_id_t ui_count_id;
	int n_perf_count;
	perf_count_id_t perf_count_ids[UI_PERF_MAP_MAX];
} ui_perf_count_map_t;

extern ui_perf_count_map_t ui_perf_count_map[UI_COUNT_NUM];

extern int get_ui_perf_count_map(ui_count_id_t, perf_count_id_t **);
extern uint64_t ui_perf_count_aggr(ui_count_id_t, uint64_t *);

#endif /* _NUMATOP_UI_PERF_MAP_H */
