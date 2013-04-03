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

#ifndef _NUMATOP_PROC_H
#define	_NUMATOP_PROC_H

#include <sys/types.h>
#include <sys/time.h>
#include <inttypes.h>
#include <pthread.h>
#include "types.h"
#include "lwp.h"
#include "node.h"
#include "perf.h"
#include "sym.h"
#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	PROC_NAME_SIZE	16
#define	PROC_HASHTBL_SIZE	128

#define	PROC_HASHTBL_INDEX(pid)	\
	((int)(pid) % PROC_HASHTBL_SIZE)

typedef struct _proc_lwplist {
	int nlwps;
	int sort_idx;
	track_lwp_t **id_arr;
	track_lwp_t **sort_arr;
} proc_lwplist_t;

typedef struct _track_proc {
	pthread_mutex_t mutex;
	int ref_count;
	boolean_t inited;
	boolean_t tagged;
	boolean_t removing;
	pid_t pid;
	int flag;
	int idarr_idx;
	int cpuid_max;
	char name[PROC_NAME_SIZE];
	proc_lwplist_t lwp_list;
	int intval_ms;
	uint64_t key;
	map_t map;
	sym_t sym;
	count_value_t *countval_arr;
	perf_countchain_t count_chain;
	perf_llrecgrp_t llrec_grp;
	struct _track_proc *hash_prev;
	struct _track_proc *hash_next;
	struct _track_proc *sort_prev;
	struct _track_proc *sort_next;
} track_proc_t;

typedef struct _proc_group {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int nprocs;
	int nlwps;
	int sort_idx;
	boolean_t inited;
	track_proc_t *hashtbl[PROC_HASHTBL_SIZE];
	track_proc_t *latest;
	track_proc_t **sort_arr;
} proc_group_t;

extern int proc_group_init(void);
extern void proc_group_fini(void);
extern track_proc_t *proc_find(pid_t);
extern track_lwp_t *proc_lwp_find(track_proc_t *, id_t);
extern void proc_lwp_count(int *, int *);
extern void proc_lwp_resort(track_proc_t *, sort_key_t);
extern void proc_group_lock(void);
extern void proc_group_unlock(void);
extern void proc_resort(sort_key_t);
extern track_proc_t *proc_sort_next(void);
extern int proc_nlwp(track_proc_t *);
extern void proc_enum_update(pid_t);
extern int proc_refcount_inc(track_proc_t *);
extern void proc_refcount_dec(track_proc_t *);
extern void proc_lwp_traverse(track_proc_t *,
	int (*func)(track_lwp_t *, void *, boolean_t *), void *);
extern int proc_countval_update(track_proc_t *, int, count_id_t, uint64_t);
extern void proc_intval_update(int);
extern int proc_intval_get(track_proc_t *);
extern void proc_profiling_clear(void);
extern void proc_ll_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* _NUMATOP_PROC_H */
