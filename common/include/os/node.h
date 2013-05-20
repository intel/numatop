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

#ifndef _NUMATOP_NODE_H
#define	_NUMATOP_NODE_H

#include <sys/types.h>
#include <inttypes.h>
#include <pthread.h>
#include "../types.h"
#include "../perf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	NODE_ALL	-1
#define INVALID_NID	-1
#define INVALID_CPUID	-1
#define	NODE_VALID(node) ((node)->nid != INVALID_NID)

typedef struct _node_meminfo {
	uint64_t mem_total;
	uint64_t mem_free;
	uint64_t active;
	uint64_t inactive;
	uint64_t dirty;
	uint64_t writeback;
	uint64_t mapped;
} node_meminfo_t;

typedef struct _node {
	int nid;
	int ncpus;
	perf_cpu_t cpus[NCPUS_NODE_MAX];
	count_value_t countval;
	node_meminfo_t meminfo;
	boolean_t hotadd;
	boolean_t hotremove;	
} node_t;

typedef struct _node_group {
	pthread_mutex_t mutex;
	node_t nodes[NNODES_MAX];
	int nnodes;
	int cpuid_max;
	int intval_ms;
	boolean_t inited;
} node_group_t;

extern int node_group_init(void);
extern void node_group_fini(void);
extern int node_group_refresh(boolean_t);
extern node_t *node_get(int);
extern int node_num(void);
extern void node_group_lock(void);
extern void node_group_unlock(void);
extern node_t *node_by_cpu(int);
extern int node_ncpus(node_t *);
extern int node_intval_get(void);
extern void node_countval_update(node_t *, count_id_t, uint64_t);
extern uint64_t node_countval_get(node_t *, count_id_t);
extern void node_meminfo(int, node_meminfo_t *);
extern int node_cpu_traverse(pfn_perf_cpu_op_t, void *, boolean_t,
	pfn_perf_cpu_op_t);
extern uint64_t node_countval_sum(count_value_t *, int, int, count_id_t);
extern perf_cpu_t* node_cpus(node_t *);
extern void node_intval_update(int);
extern void node_profiling_clear(void);
extern node_t* node_valid_get(int);
extern int node_cpuid_max(void);

#ifdef __cplusplus
}
#endif

#endif /* _NODE_H */
