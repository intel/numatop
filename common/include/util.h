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

#ifndef _NUMATOP_UTIL_H
#define	_NUMATOP_UTIL_H

#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <assert.h>
#include <pthread.h>
#include "types.h"
#include "./os/map.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	EXIT_MSG_SIZE	128
#define	LINE_SIZE		512
#define	PROCFS_ID_NUM	4096

#ifndef MIN
#define	MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define	ASSERT(expr) assert(expr)

#define	DUMP_CACHE_SIZE	256*1024
#define	LOGFILE_PATH	"/tmp/numatop.log"

typedef struct _debug_ctl {
	pthread_mutex_t mutex;
	boolean_t inited;
} debug_ctl_t;

typedef struct _dump_ctl {
	FILE *fout;
	char *cache;
	char *pcur;
	int rest_size;
	boolean_t cache_mode;
} dump_ctl_t;

extern pid_t g_numatop_pid;
extern struct timeval g_tvbase;
extern int g_pagesize;
extern double g_nsofclk;

extern void *zalloc(size_t n);
extern int debug_init(int, FILE *);
extern void debug_fini(void);
extern void debug_print(FILE *out, int level, const char *fmt, ...);
extern uint64_t current_ms(void);
extern double ratio(uint64_t value1, uint64_t value2);
extern int procfs_enum_id(char *, int **, int *);
extern int procfs_proc_enum(pid_t **, int *);
extern void exit_msg_put(const char *fmt, ...);
extern void exit_msg_print(void);
extern uint64_t cyc2ns(uint64_t);
extern int dump_init(FILE *);
extern void dump_fini(void);
extern void dump_write(const char *fmt, ...);
extern void dump_cache_enable(void);
extern void dump_cache_disable(void);
extern void dump_cache_flush(void);
extern void stderr_print(char *format, ...);
extern int array_alloc(void **, int *, int *, int, int);
extern void pagesize_init(void);
extern uint64_t rdtsc(void);

#ifdef __cplusplus
}
#endif

#endif /* _NUMATOP_UTIL_H */
