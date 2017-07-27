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

#include <stddef.h>
#include "./include/types.h"
#include "./include/ui_perf_map.h"

int
get_ui_perf_count_map(ui_count_id_t ui_count_id, perf_count_id_t **perf_count_ids)
{
	if (ui_count_id == UI_COUNT_INVALID) {
		return PERF_COUNT_INVALID;
	}

	*perf_count_ids = ui_perf_count_map[ui_count_id].perf_count_ids;
	return ui_perf_count_map[ui_count_id].n_perf_count;
}

uint64_t
ui_perf_count_aggr(ui_count_id_t ui_count_id, uint64_t *counts)
{
	int i = 0;
	uint64_t tmp = 0;
	int n_perf_count;
	perf_count_id_t *perf_count_ids = NULL;

	n_perf_count = get_ui_perf_count_map(ui_count_id, &perf_count_ids);

	for (i = 0; i < n_perf_count; i++) {
		tmp += counts[perf_count_ids[i]];
	}
	return tmp;
}
