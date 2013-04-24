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

/* This file contains code to retrieve performance data */

#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/mman.h>
#include "include/types.h"
#include "include/perf.h"
#include "include/lwp.h"
#include "include/proc.h"
#include "include/node.h"
#include "include/proc.h"
#include "include/util.h"
#include "include/plat.h"
#include "include/pfwrapper.h"
#include "include/disp.h"

typedef struct _profiling_conf {
	pf_conf_t conf_arr[COUNT_NUM];
} profiling_conf_t;

static perf_ctl_t s_perf_ctl;
static pf_profiling_rec_t *s_profiling_recbuf = NULL;
static pf_ll_rec_t *s_ll_recbuf = NULL;
static profiling_conf_t s_profiling_conf;
static pf_conf_t s_ll_conf;
static boolean_t s_partpause_enabled;

static uint64_t s_sample_period[COUNT_NUM][PRECISE_NUM] = {
	{ SMPL_PERIOD_DEFAULT_CORE_CLK, SMPL_PERIOD_MIN_CORE_CLK, SMPL_PERIOD_MAX_CORE_CLK },
	{ SMPL_PERIOD_DEFAULT_RMA, SMPL_PERIOD_MIN_RMA, SMPL_PERIOD_MAX_RMA },
	{ SMPL_PERIOD_DEFAULT_CLK, SMPL_PERIOD_MIN_CLK, SMPL_PERIOD_MAX_CLK },
	{ SMPL_PERIOD_DEFAULT_IR, SMPL_PERIOD_MIN_IR, SMPL_PERIOD_MAX_IR },
	{ SMPL_PERIOD_DEFAULT_LMA, SMPL_PERIOD_MIN_LMA, SMPL_PERIOD_MAX_LMA }
};

static boolean_t
task_valid(perf_task_t *task)
{
	switch (TASKID(task)) {
	case PERF_PROFILING_START_ID:
		/* fall through */
	case PERF_PROFILING_SMPL_ID:
		/* fall through */
	case PERF_PROFILING_PARTPAUSE_ID:
		/* fall through */
	case PERF_PROFILING_RESTORE_ID:
		/* fall through */
	case PERF_LL_START_ID:
		/* fall through */
	case PERF_LL_SMPL_ID:
		/* fall through */
	case PERF_STOP_ID:
		/* fall through */
	case PERF_QUIT_ID:
		return (B_TRUE);
	default:
		break;
	}

	return (B_FALSE);
}

static void
task_set(perf_task_t *task)
{
	(void) pthread_mutex_lock(&s_perf_ctl.mutex);
	memcpy(&s_perf_ctl.task, task, sizeof (perf_task_t));
	(void) pthread_cond_signal(&s_perf_ctl.cond);
	(void) pthread_mutex_unlock(&s_perf_ctl.mutex);
}

static void
status_set(perf_status_t status)
{
	(void) pthread_mutex_lock(&s_perf_ctl.status_mutex);
	s_perf_ctl.status = status;
	(void) pthread_cond_signal(&s_perf_ctl.status_cond);
	(void) pthread_mutex_unlock(&s_perf_ctl.status_mutex);
}

static boolean_t
status_failed(perf_status_t status)
{
	switch (status) {
	case PERF_STATUS_PROFILING_FAILED:
		/* fall through */
	case PERF_STATUS_LL_FAILED:
		return (B_TRUE);
	
	default:
		break;
	}
	
	return (B_FALSE);
}

static int
status_wait(perf_status_t status)
{
	struct timespec timeout;
	struct timeval tv;
	int s, ret = -1;

	(void) gettimeofday(&tv, NULL);
	timeout.tv_sec = tv.tv_sec + PERF_WAIT_NSEC;
	timeout.tv_nsec = tv.tv_usec * 1000;

	(void) pthread_mutex_lock(&s_perf_ctl.status_mutex);
	for (;;) {
		s = pthread_cond_timedwait(&s_perf_ctl.status_cond,
			&s_perf_ctl.status_mutex, &timeout);

		if (s_perf_ctl.status == status) {
			ret = 0;
			break;
		}

		if (status_failed(s_perf_ctl.status)) {
			break;			
		}

		if (s == ETIMEDOUT) {
			break;
		}
	}

	(void) pthread_mutex_unlock(&s_perf_ctl.status_mutex);
	return (ret);
}

static boolean_t
event_valid(perf_cpu_t *cpu)
{
	return (cpu->map_base != MAP_FAILED);
}

static void
cpu_op(perf_cpu_t *cpu, pfn_pf_event_op_t op)
{
	if (!event_valid(cpu)) {
		return;
	}

	if (op(cpu) != 0) {
		/*
		 * ioctl is failed. It might be the CPU is offline or other
		 * error occurred.
		 */
		pf_resource_free(cpu);
	}
}

static void
cpu_init(perf_cpu_t *cpu)
{
	int i;

	for (i = 0; i < COUNT_NUM; i++) {
		cpu->fds[i] = INVALID_FD;	
	}

	cpu->map_base = MAP_FAILED;
}

static int
cpu_profiling_setup(perf_cpu_t *cpu, void *arg)
{
	pf_conf_t *conf_arr = s_profiling_conf.conf_arr;
	int i, ret = 0;

	cpu_init(cpu);
	for (i = 0; i < COUNT_NUM; i++) {
		if (conf_arr[i].config == INVALID_CONFIG) {
			/*
			 * Invalid config is at the end of array.
			 */
			break;
		}

		if (pf_profiling_setup(cpu, i, &conf_arr[i]) != 0) {
			ret = -1;
			break;
		}
	}

	if (ret != 0) {
		pf_resource_free(cpu);
	}

	return (ret);
}

static int
cpu_profiling_start(perf_cpu_t *cpu, void *arg)
{
	cpu_op(cpu, pf_profiling_allstart);	
	return (0);
}

static int
cpu_profiling_stop(perf_cpu_t *cpu, void *arg)
{
	cpu_op(cpu, pf_profiling_allstop);	
	return (0);
}

static int
cpu_resource_free(perf_cpu_t *cpu, void *arg)
{	
	pf_resource_free(cpu);
	return (0);
}

static int
profiling_pause(void)
{
	node_cpu_traverse(cpu_profiling_stop, NULL, B_FALSE, NULL);
	return (0);
}

static int
profiling_stop(void)
{
	profiling_pause();
	node_cpu_traverse(cpu_resource_free, NULL, B_FALSE, NULL);
	return (0);
}

static int
profiling_start(task_profiling_t *task)
{
	/* Setup perf on each CPU. */
	if (node_cpu_traverse(cpu_profiling_setup, NULL, B_TRUE, NULL) != 0) {
		return (-1);
	}

	profiling_pause();

	/* Start to count on each CPU. */
	if (node_cpu_traverse(cpu_profiling_start, NULL, B_TRUE, NULL) != 0) {
		return (-1);
	}

	s_perf_ctl.last_ms = current_ms();
	return (0);
}

static void
countval_diff_base(perf_cpu_t *cpu, pf_profiling_rec_t *record)
{
	count_value_t *countval_last = &cpu->countval_last;
	count_value_t *countval_new = &record->countval;
	int i;

	for (i = 0; i < COUNT_NUM; i++) {
		countval_last->counts[i] = countval_new->counts[i];
	}
}

static void
countval_diff(perf_cpu_t *cpu, pf_profiling_rec_t *record,
	count_value_t *diff)
{
	count_value_t *countval_last = &cpu->countval_last;
	count_value_t *countval_new = &record->countval;
	int i;

	for (i = 0; i < COUNT_NUM; i++) {
		if (countval_last->counts[i] <= countval_new->counts[i]) {
			diff->counts[i] = countval_new->counts[i] -
				countval_last->counts[i];
		} else {
			/* Something wrong, discard it, */
			diff->counts[i] = 0;			
		}

		countval_last->counts[i] = countval_new->counts[i];	
	}
}

int
chain_add(perf_countchain_t *count_chain, int count_id, uint64_t count_value,
	uint64_t *ips, int ip_num)
{
	perf_chainrecgrp_t *grp;
	perf_chainrec_t *rec;

	grp = &count_chain->chaingrps[count_id];
	
	if (array_alloc((void **)&grp->rec_arr, &grp->nrec_cur, &grp->nrec_max,
		sizeof (perf_chainrec_t), PERF_REC_NUM) != 0) {
		return (-1);
	}

	rec = &(grp->rec_arr[grp->nrec_cur]);
	rec->count_value = count_value;
	rec->callchain.ip_num = ip_num;
	memcpy(rec->callchain.ips, ips, IP_NUM * sizeof (uint64_t));
	grp->nrec_cur++;
	return (0);
}

static int
cpu_profiling_smpl(perf_cpu_t *cpu, void *arg)
{
	pf_profiling_rec_t *record;
	track_proc_t *proc;
	track_lwp_t *lwp;
	node_t *node;
	count_value_t diff;
	int i, j, record_num;

	if (!event_valid(cpu)) {
		return (0);
	}	

	/*
	 * The record is grouped by pid/tid.
	 */
	pf_profiling_record(cpu, s_profiling_recbuf, &record_num);	
	if (record_num == 0) {
		return (0);
	}

	if ((node = node_by_cpu(cpu->cpuid)) == NULL) {
		return (0);
	}
	
	countval_diff_base(cpu, &s_profiling_recbuf[0]);

	for (i = 1; i < record_num; i++) {
		record = &s_profiling_recbuf[i];
		countval_diff(cpu, record, &diff);

		if ((proc = proc_find(record->pid)) == NULL) {
			return (0);
		}

		if ((lwp = proc_lwp_find(proc, record->tid)) == NULL) {
			proc_refcount_dec(proc);
			return (0);
		}

 		pthread_mutex_lock(&proc->mutex);
		for (j = 0; j < COUNT_NUM; j++) {
			if (!s_partpause_enabled) {
				proc_countval_update(proc, cpu->cpuid, j, diff.counts[j]);			
				lwp_countval_update(lwp, cpu->cpuid, j, diff.counts[j]);
				node_countval_update(node, j, diff.counts[j]);
			}

			if ((record->ip_num > 0) &&
				(diff.counts[j] >= s_sample_period[j][g_precise])) {					
				/*
				 * The event is overflowed. The call-chain represents
				 * the context when event is overflowed.
				 */
				chain_add(&proc->count_chain, j, diff.counts[j], record->ips,
					record->ip_num);

				chain_add(&lwp->count_chain, j, diff.counts[j], record->ips,
					record->ip_num);
			}
		}		

 		pthread_mutex_unlock(&proc->mutex);
 		lwp_refcount_dec(lwp);
 		proc_refcount_dec(proc);
	}

	return (0);
}

static int
cpu_profiling_setupstart(perf_cpu_t *cpu, void *arg)
{
	if (cpu_profiling_setup(cpu, NULL) != 0) {
		return (-1);
	}

	return (cpu_profiling_start(cpu, NULL));
}

static int
profiling_smpl(task_profiling_t *task, int *intval_ms)
{
	*intval_ms = current_ms() - s_perf_ctl.last_ms;
	proc_intval_update(*intval_ms);
	node_intval_update(*intval_ms);
	node_cpu_traverse(cpu_profiling_smpl, NULL, B_FALSE, cpu_profiling_setupstart);
	s_perf_ctl.last_ms = current_ms();
	return (0);	
}

static int
cpu_profiling_partpause(perf_cpu_t *cpu, void *arg)
{
	count_id_t keeprun_id = (count_id_t)arg;
	int i;

	if ((keeprun_id == COUNT_INVALID) || (keeprun_id == 0)) {
		return (pf_profiling_allstop(cpu));
	}
	
	for (i = 1; i < COUNT_NUM; i++) {
		if (i != keeprun_id) {
			pf_profiling_stop(cpu, i);
		} else {
			pf_profiling_start(cpu, i);	
		}
	}
	
	return (0);
}

static int
profiling_partpause(task_partpause_t *task)
{
	node_cpu_traverse(cpu_profiling_partpause,
		(void *)(task->keeprun_id), B_FALSE, NULL);

	s_partpause_enabled = B_TRUE;
	return (0);	
}

static int
cpu_profiling_restore(perf_cpu_t *cpu, void *arg)
{
	count_id_t keeprun_id = (count_id_t)arg;
	int i;

	if ((keeprun_id == COUNT_INVALID) || (keeprun_id == 0)) {
		return (pf_profiling_allstart(cpu));
	}

	pf_profiling_stop(cpu, keeprun_id);
	
	/*
	 * Discard the existing records in ring buffer.
	 */
	pf_profiling_record(cpu, NULL, NULL);	

	for (i = 1; i < COUNT_NUM; i++) {
		pf_profiling_start(cpu, i);
	}

	return (0);
}

static int
profiling_restore(task_restore_t *task)
{
	node_cpu_traverse(cpu_profiling_restore,
		(void *)(task->keeprun_id), B_FALSE, NULL);

	s_partpause_enabled = B_FALSE;
	s_perf_ctl.last_ms = current_ms();	
	return (0);	
}

static int
cpu_ll_setup(perf_cpu_t *cpu, void *arg)
{
	cpu_init(cpu);
	if (pf_ll_setup(cpu, &s_ll_conf) != 0) {
		pf_resource_free(cpu);
		return (-1);
	}

	return (0);
}

static int
cpu_ll_start(perf_cpu_t *cpu, void *arg)
{
	cpu_op(cpu, pf_ll_start);	
	return (0);
}

static int
ll_start(void)
{
	/* Setup perf on each CPU. */
	if (node_cpu_traverse(cpu_ll_setup, NULL, B_TRUE, NULL) != 0) {
		return (-1);
	}

	/* Start to count on each CPU. */
	node_cpu_traverse(cpu_ll_start, NULL, B_FALSE, NULL);

	s_perf_ctl.last_ms = current_ms();
	return (0);
}

static int
cpu_ll_stop(perf_cpu_t *cpu, void *arg)
{
	cpu_op(cpu, pf_ll_stop);	
	return (0);
}

static int
ll_stop(void)
{
	node_cpu_traverse(cpu_ll_stop, NULL, B_FALSE, NULL);
	node_cpu_traverse(cpu_resource_free, NULL, B_FALSE, NULL);
	return (0);	
}

static void
stop_all(void)
{
	if (perf_profiling_started()) {
		profiling_stop();
	}

	if (perf_ll_started()) {
		ll_stop();				
	}
}

static int
llrec_add(perf_llrecgrp_t *grp, pf_ll_rec_t *record)
{
	perf_llrec_t *llrec;

	if (array_alloc((void **)(&grp->rec_arr), &grp->nrec_cur, &grp->nrec_max,
		sizeof (perf_llrec_t), PERF_REC_NUM) != 0) {
		return (-1);
	}
	
	llrec = &(grp->rec_arr[grp->nrec_cur]);
	llrec->addr = record->addr;
	llrec->cpu = record->cpu;
	llrec->latency = record->latency;
	llrec->callchain.ip_num = record->ip_num;
	memcpy(llrec->callchain.ips, record->ips, IP_NUM * sizeof (uint64_t));
	grp->nrec_cur++;
	return (0);
}

static int
cpu_ll_smpl(perf_cpu_t *cpu, void *arg)
{
	task_ll_t *task = (task_ll_t *)arg;
	pf_ll_rec_t *record;
	track_proc_t *proc;
	track_lwp_t *lwp;
	int record_num, i;

	pf_ll_record(cpu, s_ll_recbuf, &record_num);
	if (record_num == 0) {
		return (0);
	}

	for (i = 0; i < record_num; i++) {
		record = &s_ll_recbuf[i];
		if ((task->pid != 0) && (task->pid != record->pid)) {
			continue;
		}
		
		if ((task->pid != 0) && (task->lwpid != 0) &&
			(task->lwpid != record->tid)) {
			continue;
		}

		if ((proc = proc_find(record->pid)) == NULL) {
			return (0);
		}

		if ((lwp = proc_lwp_find(proc, record->tid)) == NULL) {
			proc_refcount_dec(proc);
			return (0);
		}

		pthread_mutex_lock(&proc->mutex);

		llrec_add(&proc->llrec_grp, record);
		llrec_add(&lwp->llrec_grp, record);

 		pthread_mutex_unlock(&proc->mutex);
 		lwp_refcount_dec(lwp);
 		proc_refcount_dec(proc);		
	}

	return (0);
}

static int
cpu_ll_setupstart(perf_cpu_t *cpu, void *arg)
{
	if (cpu_ll_setup(cpu, NULL) != 0) {
		return (-1);
	}

	return (cpu_ll_start(cpu, NULL));
}

static int
ll_smpl(task_ll_t *task, int *intval_ms)
{
	*intval_ms = current_ms() - s_perf_ctl.last_ms;
	proc_intval_update(*intval_ms);
	node_cpu_traverse(cpu_ll_smpl, (void *)task, B_FALSE, cpu_ll_setupstart);
	s_perf_ctl.last_ms = current_ms();
	return (0);	
}

/*
 * The thread handler of 'perf thread'.
 */
/* ARGSUSED */
static void *
perf_handler(void *arg)
{
	perf_task_t task;
	int intval_ms;

	for (;;) {
		(void) pthread_mutex_lock(&s_perf_ctl.mutex);
		task = s_perf_ctl.task;
		while (!task_valid(&task)) {
			(void) pthread_cond_wait(&s_perf_ctl.cond, &s_perf_ctl.mutex);
			task = s_perf_ctl.task;
		}

		TASKID_SET(&s_perf_ctl.task, PERF_INVALID_ID);
		(void) pthread_mutex_unlock(&s_perf_ctl.mutex);

		switch (TASKID(&task)) {
		case PERF_QUIT_ID:
			debug_print(NULL, 2, "perf_handler: received QUIT\n");
			stop_all();
           	goto L_EXIT;

		case PERF_STOP_ID:
			stop_all();
			status_set(PERF_STATUS_IDLE);
			break;

		case PERF_PROFILING_START_ID:
			if (perf_profiling_started()) {
				status_set(PERF_STATUS_PROFILING_STARTED);
				debug_print(NULL, 2, "perf_handler: profiling started yet\n");
				break;
			}
			
			if (perf_ll_started()) {
				ll_stop();
			}

			proc_ll_clear();
			
			if (profiling_start((task_profiling_t *)(&task)) != 0) {
				debug_print(NULL, 2, "perf_handler: profiling_start failed\n");
				exit_msg_put("Fail to setup perf (probably permission denied)!\n");
				status_set(PERF_STATUS_PROFILING_FAILED);
				goto L_EXIT;
			}

			status_set(PERF_STATUS_PROFILING_STARTED);
			debug_print(NULL, 2, "perf_handler: profiling_start success\n");
			break;

		case PERF_PROFILING_SMPL_ID:
			if (!perf_profiling_started()) {
				break;	
			}

			proc_enum_update(0);
			proc_profiling_clear();
			node_profiling_clear();

			if (profiling_smpl((task_profiling_t *)(&task), &intval_ms) != 0) {
				status_set(PERF_STATUS_PROFILING_FAILED);
				disp_profiling_data_fail();
			} else {				
				disp_profiling_data_ready(intval_ms);
			}
			break;

		case PERF_PROFILING_PARTPAUSE_ID:
			profiling_partpause((task_partpause_t *)(&task));
			status_set(PERF_STATUS_PROFILING_PART_STARTED);
			break;
			
		case PERF_PROFILING_RESTORE_ID:
			proc_profiling_clear();
			profiling_restore((task_restore_t *)(&task));
			status_set(PERF_STATUS_PROFILING_STARTED);
			break;
			
		case PERF_LL_START_ID:
			if (perf_ll_started()) {
				status_set(PERF_STATUS_LL_STARTED);
				debug_print(NULL, 2, "perf_handler: ll started yet\n");
				break;
			}
			
			if (perf_profiling_started()) {
				profiling_stop();
			}

			proc_profiling_clear();
			node_profiling_clear();

			if (ll_start() != 0) {
				/*
				 * It could be failed if the kernel doesn't support PEBS LL.
				 */
				debug_print(NULL, 2, "perf_handler: ll_start is failed\n");
				status_set(PERF_STATUS_LL_FAILED);
			} else {
				debug_print(NULL, 2, "perf_handler: ll_start success\n");
				status_set(PERF_STATUS_LL_STARTED);
			}

			break;

		case PERF_LL_SMPL_ID:			
			if (!perf_ll_started()) {
				break;	
			}

			proc_enum_update(0);
			proc_ll_clear();

			if (ll_smpl((task_ll_t *)(&task), &intval_ms) != 0) {
				status_set(PERF_STATUS_LL_FAILED);
				disp_ll_data_fail();
			} else {
				disp_ll_data_ready(intval_ms);
			}
			break;

		default:
			break;
		}
	}

L_EXIT:
	debug_print(NULL, 2, "perf thread is exiting.\n");
	return (NULL);
}

static void
profiling_init(profiling_conf_t *conf)
{
	plat_event_config_t cfg;
	pf_conf_t *conf_arr = conf->conf_arr;
	int i;

	for (i = 0; i < COUNT_NUM; i++) {
		plat_profiling_config(i, &cfg);
		conf_arr[i].count_id = i;
		conf_arr[i].type = cfg.type;

		switch (conf_arr[i].type) {
		case PERF_TYPE_RAW:
			if (cfg.config != INVALID_CODE_UMASK) {
				conf_arr[i].config = (cfg.config) | (cfg.other_attr << 16);
			} else {
				conf_arr[i].config = INVALID_CONFIG;
			}
			break;
		
		case PERF_TYPE_HARDWARE:
			conf_arr[i].config = cfg.config;
			break;
			
		default:
			break;
		}

		conf_arr[i].config1 = cfg.extra_value;
		conf_arr[i].sample_period = s_sample_period[i][g_precise];
	}
}

static void
ll_init(pf_conf_t *conf)
{
	plat_event_config_t cfg;

	plat_ll_config(&cfg);
	conf->count_id = COUNT_INVALID;
	conf->type = cfg.type;
	conf->config = (cfg.config) | (cfg.other_attr << 16);
	conf->config1 = cfg.extra_value;
	conf->sample_period = LL_PERIOD;
}

/*
 * Initialization for perf control structure.
 */
int
perf_init(void)
{
	boolean_t mutex_inited = B_FALSE;
	boolean_t cond_inited = B_FALSE;
	boolean_t status_mutex_inited = B_FALSE;
	boolean_t status_cond_inited = B_FALSE;
	int ringsize, size;

	(void) memset(&s_perf_ctl, 0, sizeof (s_perf_ctl));
	if (pthread_mutex_init(&s_perf_ctl.mutex, NULL) != 0) {
		goto L_EXIT;
	}
	mutex_inited = B_TRUE;

	if (pthread_cond_init(&s_perf_ctl.cond, NULL) != 0) {
		goto L_EXIT;
	}
	cond_inited = B_TRUE;

	if (pthread_mutex_init(&s_perf_ctl.status_mutex, NULL) != 0) {
		goto L_EXIT;
	}
	status_mutex_inited = B_TRUE;

	if (pthread_cond_init(&s_perf_ctl.status_cond, NULL) != 0) {
		goto L_EXIT;
	}
	status_cond_inited = B_TRUE;

	if (pthread_create(&s_perf_ctl.thr, NULL, perf_handler, NULL) != 0) {
		goto L_EXIT;
	}

	ringsize = pf_ringsize_init();
	size = ((ringsize / sizeof (pf_profiling_rbrec_t)) + 1) *
		sizeof (pf_profiling_rec_t);

	if ((s_profiling_recbuf = zalloc(size)) == NULL) {
		goto L_EXIT;
	}

	profiling_init(&s_profiling_conf);

	size = ((ringsize / sizeof (pf_ll_rbrec_t)) + 1) *
		sizeof (pf_ll_rec_t);

	if ((s_ll_recbuf = zalloc(size)) == NULL) {
		goto L_EXIT;
	}

	ll_init(&s_ll_conf);

	s_perf_ctl.last_ms = current_ms();
	if (perf_profiling_start() != 0) {
		goto L_EXIT;
	}

	s_partpause_enabled = B_FALSE;
	s_perf_ctl.inited = B_TRUE;

L_EXIT:
	if (!s_perf_ctl.inited) {
		if (mutex_inited) {
			(void) pthread_mutex_destroy(&s_perf_ctl.mutex);
		}

		if (cond_inited) {
			(void) pthread_cond_destroy(&s_perf_ctl.cond);
		}

		if (status_mutex_inited) {
			(void) pthread_mutex_destroy(&s_perf_ctl.status_mutex);
		}

		if (status_cond_inited) {
			(void) pthread_cond_destroy(&s_perf_ctl.status_cond);
		}

		return (-1);
	}

	return (0);
}

static void
perfthr_quit_wait(void)
{
	perf_task_t task;
	task_quit_t *t;
	
	debug_print(NULL, 2, "Send PERF_QUIT_ID to perf thread\n");
	memset(&task, 0, sizeof (perf_task_t));
	t = (task_quit_t *)&task;
	t->task_id = PERF_QUIT_ID;
	task_set(&task);
	(void) pthread_join(s_perf_ctl.thr, NULL);
	debug_print(NULL, 2, "perf thread exit yet\n");
}

/*
 * Release the resources of perf control structure.
 */
void
perf_fini(void)
{
	if (s_perf_ctl.inited) {
		perfthr_quit_wait();
		(void) pthread_mutex_destroy(&s_perf_ctl.mutex);
		(void) pthread_cond_destroy(&s_perf_ctl.cond);
		(void) pthread_mutex_destroy(&s_perf_ctl.status_mutex);
		(void) pthread_cond_destroy(&s_perf_ctl.status_cond);		
		s_perf_ctl.inited = B_FALSE;
	}

	if (s_profiling_recbuf != NULL) {
		free(s_profiling_recbuf);
		s_profiling_recbuf = NULL;
	}

	if (s_ll_recbuf != NULL) {
		free(s_ll_recbuf);
		s_ll_recbuf = NULL;	
	}
}

int
perf_allstop(void)
{
	perf_task_t task;
	task_allstop_t *t;
	
	memset(&task, 0, sizeof (perf_task_t));
	t = (task_allstop_t *)&task;
	t->task_id = PERF_STOP_ID;
	task_set(&task);
	return (status_wait(PERF_STATUS_IDLE));
}

boolean_t
perf_profiling_started(void)
{
	if ((s_perf_ctl.status == PERF_STATUS_PROFILING_PART_STARTED) ||
		(s_perf_ctl.status == PERF_STATUS_PROFILING_STARTED)) {
		return (B_TRUE);
	}
	
	return (B_FALSE);
}

int
perf_profiling_start(void)
{
	perf_task_t task;
	task_profiling_t *t;
		
	memset(&task, 0, sizeof (perf_task_t));
	t = (task_profiling_t *)&task;
	t->task_id = PERF_PROFILING_START_ID;
	task_set(&task);
	return (status_wait(PERF_STATUS_PROFILING_STARTED));
}

/*
 * The user may refresh the current window frequently.
 * One refresh operation would invoke one time perf data
 * sampling. If the sampling interval is too small, the
 * counting of an event with predefined threshold probably
 * doesn't get chance to overflow. Then the sampling data
 * is not very accurate.
 *
 * For example:
 * Suppose the user refreshes the window in each 100ms. The
 * overflow threshold for RMA is 100,000. Suppose for a
 * workload, it's overflowed in each 200ms. Then the user
 * can only see the RMA is 0 after he refreshes the window.
 */
static void
smpl_wait(void)
{
	int intval_diff;

	intval_diff = current_ms() - s_perf_ctl.last_ms;
	
	if (PERF_INTVAL_MIN_MS > intval_diff) {
		intval_diff = PERF_INTVAL_MIN_MS - intval_diff;
		(void) usleep(intval_diff * USEC_MS);
	}
}

int
perf_profiling_smpl(void)
{
	perf_task_t task;
	task_profiling_t *t;
	
	smpl_wait();	
	memset(&task, 0, sizeof (perf_task_t));
	t = (task_profiling_t *)&task;
	t->task_id = PERF_PROFILING_SMPL_ID;
	task_set(&task);
	return (0);
}

int
perf_profiling_partpause(count_id_t keeprun_id)
{
	perf_task_t task;
	task_partpause_t *t;
	
	memset(&task, 0, sizeof (perf_task_t));
	t = (task_partpause_t *)&task;
	t->task_id = PERF_PROFILING_PARTPAUSE_ID;
	t->keeprun_id = keeprun_id;
	task_set(&task);
	return (status_wait(PERF_STATUS_PROFILING_PART_STARTED));
}

int
perf_profiling_restore(count_id_t keeprun_id)
{
	perf_task_t task;
	task_restore_t *t;
		
	memset(&task, 0, sizeof (perf_task_t));
	t = (task_restore_t *)&task;
	t->task_id = PERF_PROFILING_RESTORE_ID;
	t->keeprun_id = keeprun_id;
	task_set(&task);
	return (status_wait(PERF_STATUS_PROFILING_STARTED));
}

boolean_t
perf_ll_started(void)
{
	if (s_perf_ctl.status == PERF_STATUS_LL_STARTED) {
		return (B_TRUE);
	}

	return (B_FALSE);
}

int
perf_ll_start(void)
{
	perf_task_t task;
	task_ll_t *t;
		
	memset(&task, 0, sizeof (perf_task_t));
	t = (task_ll_t *)&task;
	t->task_id = PERF_LL_START_ID;
	task_set(&task);	
	return (status_wait(PERF_STATUS_LL_STARTED));
}

int
perf_ll_smpl(pid_t pid, int lwpid)
{
	perf_task_t task;
	task_ll_t *t;

	smpl_wait();
	memset(&task, 0, sizeof (perf_task_t));
	t = (task_ll_t *)&task;
	t->task_id = PERF_LL_SMPL_ID;
	t->pid = pid;
	t->lwpid = lwpid;
	task_set(&task);	
	return (0);
}

void
perf_llrecgrp_reset(perf_llrecgrp_t *grp)
{
	if (grp->rec_arr != NULL) {
		free(grp->rec_arr);	
	}

	memset(grp, 0, sizeof (perf_llrecgrp_t));
}

void
perf_cpuarr_init(perf_cpu_t *cpu_arr, int num, boolean_t hotadd)
{
	int i;
	
	for (i = 0; i < num; i++) {
		cpu_arr[i].cpuid = INVALID_CPUID;
		cpu_arr[i].hotadd = hotadd;
		cpu_init(&cpu_arr[i]);		
	}
}

void
perf_cpuarr_fini(perf_cpu_t *cpu_arr, int num, boolean_t hotremove)
{
	int i;
	
	for (i = 0; i < num; i++) {
		if (cpu_arr[i].cpuid != INVALID_CPUID) {
			cpu_arr[i].hotremove = hotremove;
		}
	}
}

static perf_cpu_t *
cpu_find(perf_cpu_t *cpu_arr, int cpu_num, int cpuid)
{
	int i;
	
	for (i = 0; i < cpu_num; i++) {
		if (cpu_arr[i].cpuid == cpuid) {
			return (&cpu_arr[i]);
		}
	}
	
	return (NULL);
}

static int
free_idx_get(perf_cpu_t *cpu_arr, int cpu_num, int prefer_idx)
{
	int i;

	if ((prefer_idx >= 0) && (prefer_idx < cpu_num)) {
		if (cpu_arr[prefer_idx].cpuid == INVALID_CPUID) {
			return (prefer_idx);
		}
	}

	for (i = 0; i < cpu_num; i++) {
		if (cpu_arr[i].cpuid == INVALID_CPUID) {
			return (i);	
		}	
	}
	
	return (-1);
}

int
perf_cpuarr_refresh(perf_cpu_t *cpu_arr, int cpu_num, int *cpuid_arr,
	int id_num, boolean_t init)
{
	int i, j, k;
	perf_cpu_t *cpu;
	
	for (i = 0; i < cpu_num; i++) {
		cpu_arr[i].hit = B_FALSE;
	}
	
	for (i = 0; i < id_num; i++) {
		if ((cpu = cpu_find(cpu_arr, cpu_num, cpuid_arr[i])) == NULL) {
			/*
			 * New CPU found.
			 */
			if ((j = free_idx_get(cpu_arr, cpu_num, i)) == -1) {
				return (-1);
			}

			cpu_arr[j].cpuid = cpuid_arr[i];
			cpu_arr[j].map_base = MAP_FAILED;
			for (k = 0; k < COUNT_NUM; k++) {
				cpu_arr[j].fds[k] = INVALID_FD;
			}

			cpu_arr[j].hit = B_TRUE;
			cpu_arr[j].hotadd = !init;

			if (cpu_arr[j].hotadd) {
				debug_print(NULL, 2, "cpu%d is hot-added.\n", cpu_arr[i].cpuid);
			}

		} else {
			cpu->hit = B_TRUE;
		}
	}

	for (i = 0; i < cpu_num; i++) {
		if ((!cpu_arr[i].hit) && (cpu_arr[i].cpuid != INVALID_CPUID)) {
			/*
			 * This CPU is invalid now.
			 */
			cpu_arr[i].hotremove = B_TRUE;
			debug_print(NULL, 2, "cpu%d is hot-removed.\n", cpu_arr[i].cpuid);
		}
	}
	
	return (0);
}

void
perf_countchain_reset(perf_countchain_t *count_chain)
{
	perf_chainrecgrp_t *grp;
	int i;
	
	for (i = 0; i < COUNT_NUM; i++) {
		grp = &count_chain->chaingrps[i];
		if (grp->rec_arr != NULL) {
			free(grp->rec_arr);	
		}
	}
	
	memset(count_chain, 0, sizeof (perf_countchain_t));
}
