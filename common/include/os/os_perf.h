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

#ifndef	_NUMATOP_OS_PERF_H
#define	_NUMATOP_OS_PERF_H

#include <sys/types.h>
#include <inttypes.h>
#include <sys/mman.h>
#include "../types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PERF_REC_NUM	512
#define PERF_FD_NUM		NCPUS_MAX * COUNT_NUM
#define INVALID_CODE_UMASK	(uint64_t)(-1)
#define PERF_PQOS_CMT_MAX	10

#define PERF_PQOS_FLAG_LLC	1
#define PERF_PQOS_FLAG_TOTAL_BW	2
#define PERF_PQOS_FLAG_LOCAL_BW	4

typedef struct _os_perf_callchain {
	unsigned int ip_num;
	uint64_t ips[IP_NUM];
} os_perf_callchain_t;

typedef struct _os_perf_llrec {
	uint64_t addr;
	uint64_t cpu;
	uint64_t latency;
	os_perf_callchain_t callchain;
} os_perf_llrec_t;

typedef struct _perf_cpu {
	int cpuid;
	int fds[COUNT_NUM];
	int group_idx;
	int map_len;
	int map_mask;
	void *map_base;
	boolean_t hit;
	boolean_t hotadd;
	boolean_t hotremove;
	count_value_t countval_last;
} perf_cpu_t;

typedef struct _perf_pqos {
	int occupancy_fd;
	int totalbw_fd;
	int localbw_fd;
	uint64_t occupancy_values[3];
	uint64_t occupancy_scaled;
	uint64_t totalbw_values[3];
	uint64_t totalbw_scaled;
	uint64_t localbw_values[3];
	uint64_t localbw_scaled;
} perf_pqos_t;

typedef int (*pfn_perf_cpu_op_t)(struct _perf_cpu *, void *);

struct _perf_ctl;
union _perf_task;
struct _perf_countchain;
struct _perf_chainrecgrp;
struct _perf_chainrec;
struct _perf_llrecgrp;
struct _track_proc;
struct _track_lwp;

extern boolean_t os_profiling_started(struct _perf_ctl *);
extern int os_profiling_start(struct _perf_ctl *, union _perf_task *);
extern int os_profiling_smpl(struct _perf_ctl *, union _perf_task *, int *);
extern int os_profiling_partpause(struct _perf_ctl *, union _perf_task *);
extern int os_profiling_restore(struct _perf_ctl *, union _perf_task *);
extern int os_callchain_start(struct _perf_ctl *, union _perf_task *);
extern int os_callchain_smpl(struct _perf_ctl *, union _perf_task *, int *);
extern int os_ll_start(struct _perf_ctl *, union _perf_task *);
extern int os_ll_smpl(struct _perf_ctl *, union _perf_task *, int *);
extern int os_perf_init(void);
extern void os_perf_fini(void);
extern void os_perfthr_quit_wait(void);
extern int os_perf_profiling_partpause(count_id_t);
extern int os_perf_profiling_restore(count_id_t);
extern int os_perf_callchain_start(pid_t, int);
extern int os_perf_callchain_smpl(void);
extern int os_perf_ll_smpl(struct _perf_ctl *, pid_t, int);
extern void os_perf_countchain_reset(struct _perf_countchain *);
extern void os_allstop(void);
extern int os_perf_allstop(void);
extern void* os_perf_priv_alloc(boolean_t *);
extern void os_perf_priv_free(void *);
extern int os_perf_chain_nentries(
    struct _perf_chainrecgrp *, int *);
extern struct _perf_chainrec* os_perf_chainrec_get(
    struct _perf_chainrecgrp *, int);
extern char *os_perf_chain_entryname(void *, int);
extern void os_perf_cpuarr_init(perf_cpu_t *, int, boolean_t);
extern void os_perf_cpuarr_fini(perf_cpu_t *, int, boolean_t);
extern int os_perf_cpuarr_refresh(perf_cpu_t *, int, int *, int, boolean_t);
extern void os_pqos_cmt_init(perf_pqos_t *);
extern int os_pqos_cmt_start(struct _perf_ctl *, union _perf_task *);
extern int os_perf_pqos_cmt_smpl(struct _perf_ctl *, pid_t, int);
extern int os_pqos_cmt_smpl(struct _perf_ctl *, union _perf_task *, int *);
extern void os_perf_pqos_free(perf_pqos_t *);
extern int os_pqos_cmt_proc_smpl(struct _track_proc *, void *, boolean_t *);
extern int os_pqos_cmt_lwp_smpl(struct _track_lwp *, void *, boolean_t *);
extern int os_pqos_cmt_proc_free(struct _track_proc *, void *, boolean_t *);
extern boolean_t os_perf_pqos_cmt_started(struct _perf_ctl *);
extern int os_pqos_proc_stop(struct _perf_ctl *, union _perf_task *);
extern int os_uncoreqpi_stop(struct _perf_ctl *, union _perf_task *);
extern int os_uncoreqpi_start(struct _perf_ctl *, union _perf_task *);
extern int os_uncoreqpi_smpl(struct _perf_ctl *, union _perf_task *, int *);
extern boolean_t os_perf_uncoreqpi_started(struct _perf_ctl *);
extern int os_perf_uncoreqpi_smpl(struct _perf_ctl *, int);

#ifdef __cplusplus
}
#endif

#endif /* _NUMATOP_OS_PERF_H */
