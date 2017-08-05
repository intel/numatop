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

#ifndef _NUMATOP_PFWRAPPER_H
#define	_NUMATOP_PFWRAPPER_H

#include <sys/types.h>
#include <pthread.h>
#include "linux/perf_event.h"
#include "../types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PF_MAP_NPAGES_MAX			1024
#define PF_MAP_NPAGES_MIN			64
#define PF_MAP_NPAGES_NORMAL		256

#if defined(__i386__)
#ifndef __NR_perf_event_open
#define __NR_perf_event_open 336
#endif

#define rmb() asm volatile("lock; addl $0,0(%%esp)" ::: "memory")
#define wmb() asm volatile("lock; addl $0,0(%%esp)" ::: "memory")
#define mb() asm volatile("lock; addl $0,0(%%esp)" ::: "memory")
#endif

#if defined(__x86_64__)
#ifndef __NR_perf_event_open
#define __NR_perf_event_open 298
#endif

#define rmb() asm volatile("lfence" ::: "memory")
#define wmb() asm volatile("sfence" ::: "memory")
#define mb() asm volatile("mfence":::"memory")
#endif

#if defined(__powerpc64__)
#ifndef __NR_perf_event_open
#define __NR_perf_event_open 319
#endif

#define rmb()  __asm__ __volatile__ ("sync" : : : "memory")
#define wmb()  __asm__ __volatile__ ("sync" : : : "memory")
#define mb()   __asm__ __volatile__ ("sync" : : : "memory")
#endif

typedef struct _pf_conf {
	perf_count_id_t perf_count_id;
	uint32_t type;
	uint64_t config;
	uint64_t config1;
	uint64_t sample_period;
} pf_conf_t;

typedef struct _pf_profiling_rec {
	unsigned int pid;
	unsigned int tid;
	uint64_t period;
	count_value_t countval;
	unsigned int ip_num;
	uint64_t ips[IP_NUM];
} pf_profiling_rec_t;

typedef struct _pf_profiling_rbrec {
	uint32_t pid;
	uint32_t tid;
	uint64_t time_enabled;
	uint64_t time_running;
	uint64_t counts[PERF_COUNT_NUM];
	uint64_t ip_num;
} pf_profiling_rbrec_t;

typedef struct _pf_ll_rec {
	unsigned int pid;
	unsigned int tid;
	uint64_t addr;
	uint64_t cpu;
	uint64_t latency;
	unsigned int ip_num;
	uint64_t ips[IP_NUM];
} pf_ll_rec_t;

typedef struct _pf_ll_rbrec {
	unsigned int pid;
	unsigned int tid;
	uint64_t addr;
	uint64_t cpu;
	uint64_t latency;
	unsigned int ip_num;
} pf_ll_rbrec_t;

struct _perf_cpu;
struct _perf_pqos;
struct _node;

typedef int (*pfn_pf_event_op_t)(struct _perf_cpu *);

int pf_ringsize_init(void);
int pf_profiling_setup(struct _perf_cpu *, int, pf_conf_t *);
int pf_profiling_start(struct _perf_cpu *, perf_count_id_t);
int pf_profiling_stop(struct _perf_cpu *, perf_count_id_t);
int pf_profiling_allstart(struct _perf_cpu *);
int pf_profiling_allstop(struct _perf_cpu *);
void pf_profiling_record(struct _perf_cpu *, pf_profiling_rec_t *, int *);
int pf_ll_setup(struct _perf_cpu *, pf_conf_t *);
int pf_ll_start(struct _perf_cpu *);
int pf_ll_stop(struct _perf_cpu *);
void pf_ll_record(struct _perf_cpu *, pf_ll_rec_t *, int *);
void pf_resource_free(struct _perf_cpu *);
int pf_pqos_occupancy_setup(struct _perf_pqos *, int pid, int lwpid);
int pf_pqos_totalbw_setup(struct _perf_pqos *, int pid, int lwpid);
int pf_pqos_localbw_setup(struct _perf_pqos *, int pid, int lwpid);
int pf_pqos_start(struct _perf_pqos *);
int pf_pqos_stop(struct _perf_pqos *);
void pf_pqos_record(struct _perf_pqos *);
void pf_pqos_resource_free(struct _perf_pqos *);
void pf_uncoreqpi_free(struct _node *);
int pf_uncoreqpi_setup(struct _node *);
int pf_uncoreqpi_start(struct _node *);
int pf_uncoreqpi_smpl(struct _node *);
void pf_uncoreimc_free(struct _node *);
int pf_uncoreimc_setup(struct _node *);
int pf_uncoreimc_start(struct _node *);
int pf_uncoreimc_smpl(struct _node *);

#ifdef __cplusplus
}
#endif

#endif /* _NUMATOP_PFWRAPPER_H */
