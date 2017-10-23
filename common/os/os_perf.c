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

#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include "../include/types.h"
#include "../include/proc.h"
#include "../include/lwp.h"
#include "../include/util.h"
#include "../include/disp.h"
#include "../include/perf.h"
#include "../include/ui_perf_map.h"
#include "../include/os/pfwrapper.h"
#include "../include/os/node.h"
#include "../include/os/plat.h"
#include "../include/os/os_perf.h"
#include "../include/os/os_util.h"

precise_type_t g_precise;

typedef struct _profiling_conf {
	pf_conf_t conf_arr[PERF_COUNT_NUM];
} profiling_conf_t;

static pf_profiling_rec_t *s_profiling_recbuf = NULL;
static pf_ll_rec_t *s_ll_recbuf = NULL;
static profiling_conf_t s_profiling_conf;
static pf_conf_t s_ll_conf;
static boolean_t s_partpause_enabled;

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

	for (i = 0; i < PERF_COUNT_NUM; i++) {
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
	for (i = 0; i < PERF_COUNT_NUM; i++) {
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

static void
countval_diff_base(perf_cpu_t *cpu, pf_profiling_rec_t *record)
{
	count_value_t *countval_last = &cpu->countval_last;
	count_value_t *countval_new = &record->countval;
	int i;

	for (i = 0; i < PERF_COUNT_NUM; i++) {
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

	for (i = 0; i < PERF_COUNT_NUM; i++) {
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

static int
chain_add(perf_countchain_t *count_chain, int perf_count_id, uint64_t count_value,
	uint64_t *ips, int ip_num)
{
	perf_chainrecgrp_t *grp;
	perf_chainrec_t *rec;

	grp = &count_chain->chaingrps[perf_count_id];
	
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

		if (record->pid == -1 || record->tid == -1)
			continue;

		countval_diff(cpu, record, &diff);

		if ((proc = proc_find(record->pid)) == NULL) {
			return (0);
		}

		if ((lwp = proc_lwp_find(proc, record->tid)) == NULL) {
			proc_refcount_dec(proc);
			return (0);
		}

 		pthread_mutex_lock(&proc->mutex);
		for (j = 0; j < PERF_COUNT_NUM; j++) {
			if (!s_partpause_enabled) {
				proc_countval_update(proc, cpu->cpuid, j, diff.counts[j]);			
				lwp_countval_update(lwp, cpu->cpuid, j, diff.counts[j]);
				node_countval_update(node, j, diff.counts[j]);
			}

			if ((record->ip_num > 0) &&
				(diff.counts[j] >= g_sample_period[j][g_precise])) {

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
cpu_profiling_partpause(perf_cpu_t *cpu, void *arg)
{
	perf_count_id_t perf_count_id = (perf_count_id_t)arg;
	int i;

	if (perf_count_id == PERF_COUNT_INVALID ||
	    perf_count_id == PERF_COUNT_CORE_CLK) {
		return (pf_profiling_allstop(cpu));
	}
	
	for (i = 1; i < PERF_COUNT_NUM; i++) {
		if (i != perf_count_id) {
			pf_profiling_stop(cpu, i);
		} else {
			pf_profiling_start(cpu, i);	
		}
	}

	return (0);
}

static int
cpu_profiling_multipause(perf_cpu_t *cpu, void *arg)
{
	perf_count_id_t *perf_count_ids = (perf_count_id_t *)arg;
	int i, j;
	boolean_t tmp[PERF_COUNT_NUM]= {B_FALSE};

	/*
	 * Prepare tmp. Each element of tmp will hold either
	 * True or False based on whether that event needs to
	 * be enabled or disabled.
	 */
	for (i = 1; i < PERF_COUNT_NUM; i++) {
		for (j = 0 ; j < UI_PERF_MAP_MAX; j++) {
			if (i == perf_count_ids[j]) {
				tmp[i] = B_TRUE;
			}
		}
	}

	for (i = 1; i < PERF_COUNT_NUM; i++) {
		if (!tmp[i]) {
			pf_profiling_stop(cpu, i);
		} else {
			pf_profiling_start(cpu, i);
		}
	}

	return (0);
}

static int
cpu_profiling_restore(perf_cpu_t *cpu, void *arg)
{
	perf_count_id_t perf_count_id = (perf_count_id_t)arg;
	int i;

	if (perf_count_id == PERF_COUNT_INVALID ||
	    perf_count_id == PERF_COUNT_CORE_CLK) {
		return (pf_profiling_allstart(cpu));
	}

	pf_profiling_stop(cpu, perf_count_id);
	
	/*
	 * Discard the existing records in ring buffer.
	 */
	pf_profiling_record(cpu, NULL, NULL);	

	for (i = 1; i < PERF_COUNT_NUM; i++) {
		pf_profiling_start(cpu, i);
	}

	return (0);
}

static int
cpu_profiling_multi_restore(perf_cpu_t *cpu, void *arg)
{
	perf_count_id_t *perf_count_ids = (perf_count_id_t *)arg;
	int i;

	for (i = 0; i < UI_PERF_MAP_MAX; i++) {
		if (perf_count_ids[i] != PERF_COUNT_INVALID &&
		    perf_count_ids[i] != PERF_COUNT_CORE_CLK) {
			pf_profiling_stop(cpu, perf_count_ids[i]);
		}
	}

	/*
	 * Discard the existing records in ring buffer.
	 */
	pf_profiling_record(cpu, NULL, NULL);

	for (i = 1; i < PERF_COUNT_NUM; i++) {
		pf_profiling_start(cpu, i);
	}

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
cpu_ll_stop(perf_cpu_t *cpu, void *arg)
{
	cpu_op(cpu, pf_ll_stop);	
	return (0);
}

static int
llrec_add(perf_llrecgrp_t *grp, pf_ll_rec_t *record)
{
	os_perf_llrec_t *llrec;

	if (array_alloc((void **)(&grp->rec_arr), &grp->nrec_cur, &grp->nrec_max,
		sizeof (os_perf_llrec_t), PERF_REC_NUM) != 0) {
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
profiling_start(perf_ctl_t *ctl, task_profiling_t *task)
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

	ctl->last_ms = current_ms(&g_tvbase);
	return (0);
}

static int
profiling_smpl(perf_ctl_t *ctl, task_profiling_t *task, int *intval_ms)
{
	*intval_ms = current_ms(&g_tvbase) - ctl->last_ms;
	proc_intval_update(*intval_ms);
	node_intval_update(*intval_ms);
	node_cpu_traverse(cpu_profiling_smpl, NULL, B_FALSE, cpu_profiling_setupstart);
	ctl->last_ms = current_ms(&g_tvbase);
	return (0);	
}

static int
profiling_partpause(perf_ctl_t *ctl, task_partpause_t *task)
{
	node_cpu_traverse(cpu_profiling_partpause,
		(void *)(task->perf_count_id), B_FALSE, NULL);

	s_partpause_enabled = B_TRUE;
	return (0);
}

static int
profiling_multipause(perf_ctl_t *ctl, task_multipause_t *task)
{
	node_cpu_traverse(cpu_profiling_multipause,
		(void *)(task->perf_count_ids), B_FALSE, NULL);

	s_partpause_enabled = B_TRUE;
	return (0);	
}

static int
profiling_restore(perf_ctl_t *ctl, task_restore_t *task)
{
	node_cpu_traverse(cpu_profiling_restore,
		(void *)(task->perf_count_id), B_FALSE, NULL);

	s_partpause_enabled = B_FALSE;
	ctl->last_ms = current_ms(&g_tvbase);
	return (0);
}

static int
profiling_multi_restore(perf_ctl_t *ctl, task_multi_restore_t *task)
{
	node_cpu_traverse(cpu_profiling_multi_restore,
		(void *)(task->perf_count_ids), B_FALSE, NULL);

	s_partpause_enabled = B_FALSE;
	ctl->last_ms = current_ms(&g_tvbase);
	return (0);	
}

static int
ll_start(perf_ctl_t *ctl)
{
	/* Setup perf on each CPU. */
	if (node_cpu_traverse(cpu_ll_setup, NULL, B_TRUE, NULL) != 0) {
		return (-1);
	}

	/* Start to count on each CPU. */
	node_cpu_traverse(cpu_ll_start, NULL, B_FALSE, NULL);

	ctl->last_ms = current_ms(&g_tvbase);
	return (0);
}

static int
ll_stop(void)
{
	node_cpu_traverse(cpu_ll_stop, NULL, B_FALSE, NULL);
	node_cpu_traverse(cpu_resource_free, NULL, B_FALSE, NULL);
	return (0);	
}

static int
ll_smpl(perf_ctl_t *ctl, task_ll_t *task, int *intval_ms)
{
	*intval_ms = current_ms(&g_tvbase) - ctl->last_ms;
	proc_intval_update(*intval_ms);
	node_cpu_traverse(cpu_ll_smpl, (void *)task, B_FALSE, cpu_ll_setupstart);
	ctl->last_ms = current_ms(&g_tvbase);
	return (0);	
}

boolean_t
os_profiling_started(perf_ctl_t *ctl)
{
	if ((ctl->status == PERF_STATUS_PROFILING_PART_STARTED) ||
		(ctl->status == PERF_STATUS_PROFILING_STARTED) ||
		(ctl->status == PERF_STATUS_PQOS_CMT_STARTED)) {
		return (B_TRUE);
	}
	
	return (B_FALSE);
}

/* ARGSUSED */
int
os_profiling_start(perf_ctl_t *ctl, perf_task_t *task)
{
	if (perf_profiling_started()) {
		perf_status_set(PERF_STATUS_PROFILING_STARTED);
		debug_print(NULL, 2, "profiling started yet\n");
		return (0);
	}

	os_allstop();
	proc_ll_clear(NULL);

	if (profiling_start(ctl, (task_profiling_t *)(task)) != 0) {
		exit_msg_put("Fail to setup perf (probably permission denied)!\n");
		debug_print(NULL, 2, "os_profiling_start failed\n");
		perf_status_set(PERF_STATUS_PROFILING_FAILED);
		return (-1);
	}

	debug_print(NULL, 2, "os_profiling_start success\n");	
	perf_status_set(PERF_STATUS_PROFILING_STARTED);
	return (0);
}

int
os_profiling_smpl(perf_ctl_t *ctl, perf_task_t *task, int *intval_ms)
{
	task_profiling_t *t = (task_profiling_t *)task;
	int ret = -1;

/*
	if (!perf_profiling_started()) {
		return (-1);
	}
*/
	proc_enum_update(0);
	proc_callchain_clear();
	proc_profiling_clear();
	node_profiling_clear();

	if (profiling_smpl(ctl, t, intval_ms) != 0) {
		perf_status_set(PERF_STATUS_PROFILING_FAILED);
		goto L_EXIT;
	}

	ret = 0;

L_EXIT:
	if (ret == 0)
		if (t->use_dispflag1)
			disp_profiling_data_ready(*intval_ms);
		else
			disp_flag2_set(DISP_FLAG_PROFILING_DATA_READY);
	else
		if (t->use_dispflag1)
			disp_profiling_data_fail();
		else
			disp_flag2_set(DISP_FLAG_PROFILING_DATA_FAIL);

	return (ret);
}

int
os_profiling_partpause(perf_ctl_t *ctl, perf_task_t *task)
{
	profiling_partpause(ctl, (task_partpause_t *)(task));
	perf_status_set(PERF_STATUS_PROFILING_PART_STARTED);
	return (0);
}

int
os_profiling_multipause(perf_ctl_t *ctl, perf_task_t *task)
{
	profiling_multipause(ctl, (task_multipause_t *)(task));
	perf_status_set(PERF_STATUS_PROFILING_MULTI_STARTED);
	return (0);
}

int
os_profiling_restore(perf_ctl_t *ctl, perf_task_t *task)
{
	proc_callchain_clear();
	proc_profiling_clear();
	profiling_restore(ctl, (task_restore_t *)(task));
	perf_status_set(PERF_STATUS_PROFILING_STARTED);
	return (0);	
}

int
os_profiling_multi_restore(perf_ctl_t *ctl, perf_task_t *task)
{
	proc_callchain_clear();
	proc_profiling_clear();
	profiling_multi_restore(ctl, (task_multi_restore_t *)(task));
	perf_status_set(PERF_STATUS_PROFILING_STARTED);
	return (0);
}

int
os_callchain_start(perf_ctl_t *ctl, perf_task_t *task)
{
	/* Not supported in Linux. */
	return (0);
}

int
os_callchain_smpl(perf_ctl_t *ctl, perf_task_t *task, int *intval_ms)
{
	/* Not supported in Linux. */
	return (0);	
}

int
os_ll_start(perf_ctl_t *ctl, perf_task_t *task)
{
	os_allstop();
	proc_callchain_clear();
	proc_profiling_clear();
	node_profiling_clear();

	if (ll_start(ctl) != 0) {
		/*
		 * It could be failed if the kernel doesn't support PEBS LL.
		 */
		debug_print(NULL, 2, "ll_start is failed\n");
		perf_status_set(PERF_STATUS_LL_FAILED);
		return (-1);
	}

	debug_print(NULL, 2, "ll_start success\n");
	perf_status_set(PERF_STATUS_LL_STARTED);
	return (0);
}

int
os_ll_smpl(perf_ctl_t *ctl, perf_task_t *task, int *intval_ms)
{
	if (!perf_ll_started()) {
		return (-1);
	}

	proc_enum_update(0);
	proc_ll_clear(0);

	if (ll_smpl(ctl, (task_ll_t *)(task), intval_ms) != 0) {
		perf_status_set(PERF_STATUS_LL_FAILED);
		disp_ll_data_fail();
		return (-1);
	}

	disp_ll_data_ready(*intval_ms);
	return (0);
}

static void
profiling_init(profiling_conf_t *conf)
{
	plat_event_config_t cfg;
	pf_conf_t *conf_arr = conf->conf_arr;
	int i;

	for (i = 0; i < PERF_COUNT_NUM; i++) {
		plat_profiling_config(i, &cfg);
		conf_arr[i].perf_count_id = i;
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
		conf_arr[i].sample_period = g_sample_period[i][g_precise];
	}
}

static void
ll_init(pf_conf_t *conf)
{
	plat_event_config_t cfg;

	plat_ll_config(&cfg);
	conf->perf_count_id = PERF_COUNT_INVALID;
	conf->type = cfg.type;
	conf->config = (cfg.config) | (cfg.other_attr << 16);
	conf->config1 = cfg.extra_value;
	conf->sample_period = LL_PERIOD;
}

int
os_perf_init(void)
{
	int ringsize, size;

	s_profiling_recbuf = NULL;
	s_ll_recbuf = NULL;
	s_partpause_enabled = B_FALSE;

	ringsize = pf_ringsize_init();
	size = ((ringsize / sizeof (pf_profiling_rbrec_t)) + 1) *
		sizeof (pf_profiling_rec_t);

	if ((s_profiling_recbuf = zalloc(size)) == NULL) {
		return (-1);
	}

	profiling_init(&s_profiling_conf);

	size = ((ringsize / sizeof (pf_ll_rbrec_t)) + 1) *
		sizeof (pf_ll_rec_t);

	if ((s_ll_recbuf = zalloc(size)) == NULL) {
		free(s_profiling_recbuf);
		s_profiling_recbuf = NULL;
		return (-1);
	}

	ll_init(&s_ll_conf);
	return (0);	
}

void
os_perf_fini(void)
{
	if (s_profiling_recbuf != NULL) {
		free(s_profiling_recbuf);
		s_profiling_recbuf = NULL;
	}

	if (s_ll_recbuf != NULL) {
		free(s_ll_recbuf);
		s_ll_recbuf = NULL;	
	}
}

void
os_perfthr_quit_wait(void)
{
	/* Not supported in Linux. */
}

int
os_perf_profiling_partpause(perf_count_id_t perf_count_id)
{
	perf_task_t task;
	task_partpause_t *t;
	
	memset(&task, 0, sizeof (perf_task_t));
	t = (task_partpause_t *)&task;
	t->task_id = PERF_PROFILING_PARTPAUSE_ID;
	t->perf_count_id = perf_count_id;
	perf_task_set(&task);
	return (perf_status_wait(PERF_STATUS_PROFILING_PART_STARTED));
}

int
os_perf_profiling_multipause(perf_count_id_t *perf_count_ids)
{
	perf_task_t task;
	task_multipause_t *t;

	memset(&task, 0, sizeof (perf_task_t));
	t = (task_multipause_t *)&task;
	t->task_id = PERF_PROFILING_MULTIPAUSE_ID;
	t->perf_count_ids = perf_count_ids;
	perf_task_set(&task);
	return (perf_status_wait(PERF_STATUS_PROFILING_MULTI_STARTED));
}

int
os_perf_profiling_restore(perf_count_id_t perf_count_id)
{
	perf_task_t task;
	task_restore_t *t;
		
	memset(&task, 0, sizeof (perf_task_t));
	t = (task_restore_t *)&task;
	t->task_id = PERF_PROFILING_RESTORE_ID;
	t->perf_count_id = perf_count_id;
	perf_task_set(&task);
	return (perf_status_wait(PERF_STATUS_PROFILING_STARTED));
}

int
os_perf_profiling_multi_restore(perf_count_id_t *perf_count_ids)
{
	perf_task_t task;
	task_multi_restore_t *t;

	memset(&task, 0, sizeof (perf_task_t));
	t = (task_multi_restore_t *)&task;
	t->task_id = PERF_PROFILING_MULTI_RESTORE_ID;
	t->perf_count_ids = perf_count_ids;
	perf_task_set(&task);
	return (perf_status_wait(PERF_STATUS_PROFILING_STARTED));	
}

int
os_perf_callchain_start(pid_t pid, int lwpid)
{
	/* Not supported in Linux. */
	return (0);
}

int
os_perf_callchain_smpl(void)
{
	/* Not supported in Linux. */
	return (0);	
}

int
os_perf_ll_smpl(perf_ctl_t *ctl, pid_t pid, int lwpid)
{
	perf_task_t task;
	task_ll_t *t;

	perf_smpl_wait();
	memset(&task, 0, sizeof (perf_task_t));
	t = (task_ll_t *)&task;
	t->task_id = PERF_LL_SMPL_ID;
	t->pid = pid;
	t->lwpid = lwpid;
	perf_task_set(&task);
	return (0);
}

void
os_perf_countchain_reset(perf_countchain_t *count_chain)
{
	perf_chainrecgrp_t *grp;
	int i;
	
	for (i = 0; i < PERF_COUNT_NUM; i++) {
		grp = &count_chain->chaingrps[i];
		if (grp->rec_arr != NULL) {
			free(grp->rec_arr);	
		}
	}
	
	memset(count_chain, 0, sizeof (perf_countchain_t));
}

static int
uncore_stop_all(void)
{
	node_t *node;
	int i;

	for (i = 0; i < NNODES_MAX; i++) {
		node = node_get(i);
		if (NODE_VALID(node)) {
			if (node->qpi.qpi_num > 0)
				pf_uncoreqpi_free(node);
				
			if (node->imc.imc_num > 0)
				pf_uncoreimc_free(node);
		}
	}

	return 0;
}

void
os_allstop(void)
{
	if (perf_profiling_started()) {
		profiling_stop();
	}

	if (perf_ll_started()) {
		ll_stop();				
	}

	if (perf_pqos_cmt_started()) {
		proc_pqos_func(NULL, os_pqos_cmt_proc_free);
	}

	if (perf_uncore_started()) {
		uncore_stop_all();
	}
}

int
os_perf_allstop(void)
{
	perf_task_t task;
	task_allstop_t *t;
	
	memset(&task, 0, sizeof (perf_task_t));
	t = (task_allstop_t *)&task;
	t->task_id = PERF_STOP_ID;
	perf_task_set(&task);
	return (perf_status_wait(PERF_STATUS_IDLE));
}

void *
os_perf_priv_alloc(boolean_t *supported)
{
	/* Not supported in Linux. */
	*supported = B_FALSE;
	return (NULL);
}

void
os_perf_priv_free(void *priv)
{
	/* Not supported in Linux. */
}

int
os_perf_chain_nentries(perf_chainrecgrp_t *grp, int *nchains)
{
	/* Not supported in Linux. */
	return (0);
}

perf_chainrec_t *
os_perf_chainrec_get(perf_chainrecgrp_t *grp, int rec_idx)
{
	/* Not supported in Linux. */
	return (NULL);
}

char *
os_perf_chain_entryname(void *buf, int depth_idx)
{
	/* Not supported in Linux. */
	return (NULL);
}

void
os_perf_cpuarr_init(perf_cpu_t *cpu_arr, int num, boolean_t hotadd)
{
	int i;
	
	for (i = 0; i < num; i++) {
		cpu_arr[i].cpuid = INVALID_CPUID;
		cpu_arr[i].hotadd = hotadd;
		cpu_init(&cpu_arr[i]);		
	}
}

void
os_perf_cpuarr_fini(perf_cpu_t *cpu_arr, int num, boolean_t hotremove)
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
os_perf_cpuarr_refresh(perf_cpu_t *cpu_arr, int cpu_num, int *cpuid_arr,
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
			for (k = 0; k < PERF_COUNT_NUM; k++) {
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

static int
pqos_cmt_start(perf_ctl_t *ctl, int pid, int lwpid, int flags)
{
	track_proc_t *proc;
	track_lwp_t *lwp = NULL;
	perf_pqos_t *pqos;
	int ret;

	if ((proc = proc_find(pid)) == NULL)
		return -1;

	if (lwpid == 0) {
		pqos = &proc->pqos;
	} else {
		if ((lwp = proc_lwp_find(proc, lwpid)) == NULL) {
			proc_refcount_dec(proc);
			return -1;
		}

		pqos = &lwp->pqos;
		proc->lwp_pqosed = B_TRUE;
	}

	memset(pqos, 0, sizeof(perf_pqos_t));
	pqos->flags = flags;

	ret = os_sysfs_cmt_task_set(pid, lwpid, pqos);

	if (lwp != NULL)
		lwp_refcount_dec(lwp);

	proc_refcount_dec(proc);
	return ret;
}

void os_pqos_cmt_init(perf_pqos_t *pqos)
{

}

int
os_pqos_cmt_start(perf_ctl_t *ctl, perf_task_t *task)
{
	task_pqos_cmt_t *t = (task_pqos_cmt_t *)task;

	if (pqos_cmt_start(ctl, t->pid, t->lwpid, t->flags) != 0) {
		debug_print(NULL, 2,
			"os_pqos_cmt_start is failed for %d/%d\n",
			t->pid, t->lwpid);
		perf_status_set(PERF_STATUS_PQOS_CMT_FAILED);
		return (-1);
	}

	perf_status_set(PERF_STATUS_PQOS_CMT_STARTED);
	return (0);
}

int
os_perf_pqos_cmt_smpl(perf_ctl_t *ctl, pid_t pid, int lwpid)
{
	perf_task_t task;
	task_pqos_cmt_t *t;

	perf_smpl_wait();
	memset(&task, 0, sizeof (perf_task_t));
	t = (task_pqos_cmt_t *)&task;
	t->task_id = PERF_PQOS_CMT_SMPL_ID;
	t->pid = pid;
	t->lwpid = lwpid;
	perf_task_set(&task);
	return (0);
}

int
os_pqos_cmt_smpl(perf_ctl_t *ctl, perf_task_t *task, int *intval_ms)
{
	task_pqos_cmt_t *t = (task_pqos_cmt_t *)task;
	track_proc_t *proc;
	track_lwp_t *lwp = NULL;
	boolean_t end;

	proc_enum_update(0);

	if (t->pid == 0)
		proc_pqos_func(NULL, os_pqos_cmt_proc_smpl);
	else {
		if ((proc = proc_find(t->pid)) == NULL) {
			disp_pqos_cmt_data_ready(0);
			return -1;
		}

		if (t->lwpid == 0)
			os_pqos_cmt_proc_smpl(proc, NULL, &end);
		else {
			if ((lwp = proc_lwp_find(proc, t->lwpid)) == NULL) {
				proc_refcount_dec(proc);
				disp_pqos_cmt_data_ready(0);
				return -1;
			}

			os_pqos_cmt_lwp_smpl(lwp, NULL, &end);
		}

		if (lwp != NULL)
			lwp_refcount_dec(lwp);

		proc_refcount_dec(proc);
	}

	*intval_ms = current_ms(&g_tvbase) - ctl->last_ms_pqos;
	ctl->last_ms_pqos = current_ms(&g_tvbase);
	disp_pqos_cmt_data_ready(*intval_ms);

	return (0);
}

void
os_perf_pqos_free(perf_pqos_t *pqos)
{
	pf_pqos_resource_free(pqos);
}

static int pqos_record(struct _perf_pqos *pqos)
{
	if (pqos->task_id == 0)
		return 0;

	return (os_sysfs_cmt_task_value(pqos, -1));
}

int os_pqos_cmt_proc_smpl(struct _track_proc *proc, void *arg, boolean_t *end)
{
	*end = B_FALSE;
	pqos_record(&proc->pqos);
	return 0;
}

int
os_pqos_cmt_lwp_smpl(track_lwp_t *lwp, void *arg, boolean_t *end)
{
	*end = B_FALSE;
	pqos_record(&lwp->pqos);
	return 0;
}

static int
os_pqos_cmt_lwp_free(track_lwp_t *lwp, void *arg, boolean_t *end)
{
	*end = B_FALSE;
	/* os_perf_pqos_free(&lwp->pqos); */
	return 0;
}

int os_pqos_cmt_proc_free(struct _track_proc *proc, void *arg, boolean_t *end)
{
	*end = B_FALSE;
	/* os_perf_pqos_free(&proc->pqos); */

	if (proc->lwp_pqosed) {
		proc_lwp_traverse(proc, os_pqos_cmt_lwp_free, NULL);
		proc->lwp_pqosed = B_FALSE;
	}

	return 0;
}

boolean_t
os_perf_pqos_cmt_started(perf_ctl_t *ctl)
{
	if (ctl->status == PERF_STATUS_PQOS_CMT_STARTED)
		return (B_TRUE);

	return (B_FALSE);
}

int os_pqos_proc_stop(perf_ctl_t *ctl, perf_task_t *task)
{
	task_pqos_cmt_t *t = (task_pqos_cmt_t *)task;
	track_proc_t *proc;
	track_lwp_t *lwp = NULL;
	boolean_t end;

	if (t->pid == 0)
		proc_pqos_func(NULL, os_pqos_cmt_proc_free);
	else {
		if ((proc = proc_find(t->pid)) == NULL)
			return -1;

		if (t->lwpid == 0)
			os_pqos_cmt_proc_free(proc, NULL, &end);
		else {
			if ((lwp = proc_lwp_find(proc, t->lwpid)) == NULL) {
				proc_refcount_dec(proc);
				return -1;
			}

			os_pqos_cmt_lwp_free(lwp, NULL, &end);
		}

		if (lwp != NULL)
			lwp_refcount_dec(lwp);

		proc_refcount_dec(proc);
	}

	return (0);
}

int os_uncore_stop(perf_ctl_t *ctl, perf_task_t *task)
{
	task_uncore_t *t = (task_uncore_t *)task;
	node_t *node;
	int i;
	
	if (t->nid >= 0) {
		node = node_get(t->nid);
		if (NODE_VALID(node)) {
			if (node->qpi.qpi_num > 0)
				pf_uncoreqpi_free(node);
			
			if (node->imc.imc_num > 0)
				pf_uncoreimc_free(node);
		}
	} else {
		for (i = 0; i < NNODES_MAX; i++) {
			node = node_get(i);
			if (NODE_VALID(node)) {
				if (node->qpi.qpi_num > 0)
					pf_uncoreqpi_free(node);

				if (node->imc.imc_num > 0)
					pf_uncoreimc_free(node);
			}
		}
	}

	return 0;
}

static int uncore_start(perf_ctl_t *ctl, int nid)
{
	node_t *node;
	int ret = -1;
	
	node = node_get(nid);
	if (!NODE_VALID(node))
		return -1;
	
	if (pf_uncoreqpi_setup(node) != 0)
		goto L_EXIT;
	
	if (pf_uncoreimc_setup(node) != 0)
		goto L_EXIT;

	if (pf_uncoreqpi_start(node) != 0)
		goto L_EXIT;
	
	if (pf_uncoreimc_start(node) != 0)
		goto L_EXIT;

	ret = 0;

L_EXIT:
	if (ret < 0) {
		pf_uncoreqpi_free(node);
		pf_uncoreimc_free(node);
	}

	return ret;
}

int
os_uncore_start(perf_ctl_t *ctl, perf_task_t *task)
{
	task_uncore_t *t = (task_uncore_t *)task;

	if (uncore_start(ctl, t->nid) != 0) {
		debug_print(NULL, 2,
			"os_uncore_start is failed for node %d/%d\n",
			t->nid);
		perf_status_set(PERF_STATUS_UNCORE_FAILED);
		return (-1);
	}

	perf_status_set(PERF_STATUS_UNCORE_STARTED);
	return (0);
}

int
os_uncore_smpl(perf_ctl_t *ctl, perf_task_t *task, int *intval_ms)
{
	task_uncore_t *t = (task_uncore_t *)task;
	node_t *node;
	int ret;

	node = node_get(t->nid);
	if (!NODE_VALID(node))
		return -1;

	ret = pf_uncoreqpi_smpl(node);
	if (ret != 0) {
		disp_profiling_data_fail();
		return ret;
	}

	ret = pf_uncoreimc_smpl(node);
	if (ret == 0)
		disp_profiling_data_ready(*intval_ms);
	else
		disp_profiling_data_fail();

	return (ret);
}

boolean_t
os_perf_uncore_started(perf_ctl_t *ctl)
{
	if (ctl->status == PERF_STATUS_UNCORE_STARTED)
		return (B_TRUE);

	return (B_FALSE);
}

int
os_perf_uncore_smpl(perf_ctl_t *ctl, int nid)
{
	perf_task_t task;
	task_uncore_t *t;

	perf_smpl_wait();
	memset(&task, 0, sizeof (perf_task_t));
	t = (task_uncore_t *)&task;
	t->task_id = PERF_UNCORE_SMPL_ID;
	t->nid = nid;

	perf_task_set(&task);
	return (0);
}
