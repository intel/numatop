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

#ifndef _NUMATOP_DISP_H
#define	_NUMATOP_DISP_H

#include <sys/types.h>
#include <sys/time.h>
#include <inttypes.h>
#include <pthread.h>
#include "types.h"
#include "util.h"
#include "cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	DISP_DEFAULT_INTVAL	5
#define	PIPE_CHAR_QUIT		'q'
#define	PIPE_CHAR_RESIZE	'r'

typedef enum {
	DISP_FLAG_NONE = 0,
	DISP_FLAG_QUIT,
	DISP_FLAG_PROFILING_DATA_READY,
	DISP_FLAG_PROFILING_DATA_FAIL,
	DISP_FLAG_CALLCHAIN_DATA_READY,
	DISP_FLAG_CALLCHAIN_DATA_FAIL,
	DISP_FLAG_LL_DATA_READY,
	DISP_FLAG_LL_DATA_FAIL,
	DISP_FLAG_PQOS_CMT_READY,
	DISP_FLAG_PQOS_CMT_FAIL,
	DISP_FLAG_CMD,
	DISP_FLAG_SCROLLUP,
	DISP_FLAG_SCROLLDOWN,
	DISP_FLAG_SCROLLENTER
} disp_flag_t;

typedef struct _disp_ctl {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_mutex_t mutex2;
	pthread_cond_t cond2;
	pthread_t thr;
	boolean_t inited;
	cmd_t cmd;
	disp_flag_t flag;
	disp_flag_t flag2;
	int intval_ms;
} disp_ctl_t;

typedef struct _cons_ctl {
	fd_set fds;
	pthread_t thr;
	int pipe[2];
	boolean_t inited;
} cons_ctl_t;

extern int g_run_secs;

extern int disp_init(void);
extern void disp_fini(void);
extern int disp_cons_ctl_init(void);
extern void disp_cons_ctl_fini(void);
extern void disp_consthr_quit(void);
extern void disp_profiling_data_ready(int);
extern void disp_profiling_data_fail(void);
extern void disp_callchain_data_ready(int);
extern void disp_callchain_data_fail(void);
extern void disp_ll_data_ready(int);
extern void disp_ll_data_fail(void);
extern void disp_pqos_cmt_data_ready(int);
extern void disp_pqos_cmt_data_fail(void);
extern void disp_on_resize(int);
extern void disp_intval(char *, int);
extern void disp_dispthr_quit_wait(void);
extern void disp_dispthr_quit_start(void);
extern void disp_go_home(void);
extern void disp_flag2_set(disp_flag_t);
extern disp_flag_t disp_flag2_wait(void);

#ifdef __cplusplus
}
#endif

#endif /* _NUMATOP_DISP_H */
