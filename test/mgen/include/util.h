/*
 * Copyright (c) 2013, Intel Corporation
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

#ifndef _NUMATOP_TEST_UTIL_H
#define _NUMATOP_TEST_UTIL_H

#ifndef PATH_MAX
#define PATH_MAX	4096
#endif

#define CPU0_CPUFREQ_PATH \
	"/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq"

#define CPUINFO_PATH \
	"/proc/cpuinfo"

#define MEAS_TIME_DEFAULT	5
#define MAX_VALUE		4294967295U
#define MHZ			1000000
#define GHZ			1000000000
#define KHZ			1000
#define NS_SEC			1000000000
#define NS_MS			1000000
#define MS_SEC			1000
#define MICROSEC		1000000
#define BUF_SIZE		256 * 1024 * 1024
#define RAND_ARRAY_SIZE		8192
#define INVALID_RAND		-1
#define BUF_ELE_SIZE		64
#define READ_NUM		10240000

extern double s_nsofclk;
extern uint64_t s_clkofsec;
extern double s_latest_avglat;
extern struct timeval s_tvbase;

extern void arch__dependent_read(void *, int);

#endif /* _NUMATOP_TEST_UTIL_H */
