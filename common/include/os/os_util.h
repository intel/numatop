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

#ifndef _NUMATOP_OS_UTIL_H
#define	_NUMATOP_OS_UTIL_H

#include <sys/types.h>

#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <assert.h>
#include <pthread.h>
#include "../types.h"
#include "../os/node.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DIGIT_LEN_MAX	512
#define CPU0_CPUFREQ_PATH \
	"/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq"
#define NODE_INFO_ROOT \
	"/sys/devices/system/node/"
#define NODE_NONLINE_PATH \
	"/sys/devices/system/node/online"
#define CPUINFO_PATH \
	"/proc/cpuinfo"
#define CQM_LLC_OCCUPANCY_SCALE_PATH \
	"/sys/devices/intel_cqm/events/llc_occupancy.scale"
#define CQM_LLC_TOTAL_BW_SCALE_PATH \
	"/sys/devices/intel_cqm/events/total_bytes.scale"
#define CQM_LLC_LOCAL_BW_SCALE_PATH \
	"/sys/devices/intel_cqm/events/local_bytes.scale"

struct _perf_pqos;

extern boolean_t os_authorized(void);
extern int os_numatop_lock(boolean_t *);
extern void os_numatop_unlock(void);
extern int os_procfs_psinfo_get(pid_t, void *);
extern int os_procfs_pname_get(pid_t, char *, int);
extern int os_procfs_lwp_enum(pid_t, int **lwps, int *);
extern boolean_t os_procfs_lwp_valid(pid_t, int);
extern int processor_bind(int cpu);
extern int processor_unbind(void);
extern void os_calibrate(double *nsofclk, uint64_t *clkofsec);
extern boolean_t os_sysfs_node_enum(int *, int, int *);
extern boolean_t os_sysfs_cpu_enum(int, int *, int, int *);
extern int sysfs_os_online_ncpus(void);
extern boolean_t os_sysfs_meminfo(int, node_meminfo_t *);
extern int os_sysfs_online_ncpus(void);
extern int os_sysfs_cqm_llc_scale(const char*, double *);
extern int os_sysfs_uncore_qpi_init(qpi_info_t *, int);
extern int os_sysfs_uncore_upi_init(qpi_info_t *, int);
extern int os_sysfs_uncore_imc_init(imc_info_t *, int);
extern boolean_t os_cmt_init(void);
extern void os_cmt_fini(void);
extern int os_sysfs_cmt_task_set(int, int, struct _perf_pqos *);
extern int os_sysfs_cmt_task_value(struct _perf_pqos *, int);

#ifdef __cplusplus
}
#endif

#endif /* _NUMATOP_OS_UTIL_H */
