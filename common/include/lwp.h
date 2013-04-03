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

#ifndef _NUMATOP_LWP_H
#define	_NUMATOP_LWP_H

#include <sys/types.h>
#include <sys/time.h>
#include <inttypes.h>
#include <pthread.h>
#include "types.h"
#include "node.h"
#include "perf.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _track_proc;

typedef struct _track_lwp {
	pthread_mutex_t mutex;
	int ref_count;
	int id;
	int intval_ms;
	int cpuid_max;
	uint64_t key;
	boolean_t removing;
	boolean_t inited;
	struct _track_proc *proc;
	count_value_t *countval_arr;
	perf_countchain_t count_chain;
	perf_llrecgrp_t llrec_grp;
} track_lwp_t;

extern int lwp_free(track_lwp_t *);
extern track_lwp_t *lwp_sort_next(struct _track_proc *);
extern void lwp_enum_update(struct _track_proc *);
extern track_lwp_t *lwp_find(pid_t, int, boolean_t, boolean_t);
extern int lwp_refcount_inc(track_lwp_t *);
extern void lwp_refcount_dec(track_lwp_t *);
extern int lwp_key_compute(track_lwp_t *, void *, boolean_t *end);
extern int lwp_countval_update(track_lwp_t *, int, count_id_t, uint64_t);
extern int lwp_intval_get(track_lwp_t *);
extern void lwp_intval_update(struct _track_proc *, int intval_ms);

#ifdef __cplusplus
}
#endif

#endif /* _NUMATOP_LWP_H */
