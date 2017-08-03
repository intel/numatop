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

/* This file contains code to handle the 'tracked process'. */

#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include "include/types.h"
#include "include/lwp.h"
#include "include/proc.h"
#include "include/disp.h"
#include "include/util.h"
#include "include/perf.h"
#include "include/os/node.h"
#include "include/os/os_util.h"

static proc_group_t s_proc_group;

/*
 * Initialization for the process group.
 */
int
proc_group_init(void)
{
	(void) memset(&s_proc_group, 0, sizeof (s_proc_group));
	if (pthread_mutex_init(&s_proc_group.mutex, NULL) != 0) {
		return (-1);
	}

	if (pthread_cond_init(&s_proc_group.cond, NULL) != 0) {
		(void) pthread_mutex_destroy(&s_proc_group.mutex);
		return (-1);
	}

	s_proc_group.inited = B_TRUE;
	return (0);
}

/* ARGSUSED */
static int
lwp_free_walk(track_lwp_t *lwp, void *arg, boolean_t *end)
{
	*end = B_FALSE;
	(void) lwp_free(lwp);
	return (0);
}

/*
 * Free resources of 'track_proc_t' if 'ref_count' is 0.
 */
static void
proc_free(track_proc_t *proc)
{
	proc_lwplist_t *list = &proc->lwp_list;

	(void) pthread_mutex_lock(&proc->mutex);
	if (proc->ref_count > 0) {
		proc->removing = B_TRUE;
		(void) pthread_mutex_unlock(&proc->mutex);
		return;
	}

	proc_lwp_traverse(proc, lwp_free_walk, NULL);
	s_proc_group.nprocs--;
	s_proc_group.nlwps -= list->nlwps;

	if (list->id_arr != NULL) {
		free(list->id_arr);
	}

	if (list->sort_arr != NULL) {
		free(list->sort_arr);
	}

	if (proc->countval_arr != NULL) {
		free(proc->countval_arr);
	}

	(void) map_proc_fini(proc);
	sym_free(&proc->sym);
	perf_countchain_reset(&proc->count_chain);
	perf_llrecgrp_reset(&proc->llrec_grp);

	os_perf_pqos_free(&proc->pqos);

	(void) pthread_mutex_unlock(&proc->mutex);
	(void) pthread_mutex_destroy(&proc->mutex);
	free(proc);
}

/*
 * Walk through all processes and call 'func()' for each processes.
 */
static void
proc_traverse(int (*func)(track_proc_t *, void *, boolean_t *), void *arg)
{
	track_proc_t *proc, *hash_next;
	boolean_t end;
	int i, j = 0;

	/*
	 * The mutex of s_proc_group has been taken outside.
	 */
	for (i = 0; i < PROC_HASHTBL_SIZE; i++) {
		proc = s_proc_group.hashtbl[i];
		while (proc != NULL) {
			j++;
			hash_next = proc->hash_next;
			func(proc, arg, &end);
			if (end) {
				return;
			}

			proc = hash_next;
		}

		if (j == s_proc_group.nprocs) {
			return;
		}
	}
}

/* ARGSUSED */
static int
proc_free_walk(track_proc_t *proc, void *arg, boolean_t *end)
{
	*end = B_FALSE;
	proc_free(proc);
	return (0);
}

/*
 * Free all the resources of process group.
 */
void
proc_group_fini(void)
{
	if (!s_proc_group.inited) {
		return;
	}

	(void) pthread_mutex_lock(&s_proc_group.mutex);
	proc_traverse(proc_free_walk, NULL);
	if (s_proc_group.sort_arr != NULL) {
		free(s_proc_group.sort_arr);
	}

	(void) pthread_mutex_unlock(&s_proc_group.mutex);
	(void) pthread_mutex_destroy(&s_proc_group.mutex);
	(void) pthread_cond_destroy(&s_proc_group.cond);
}

/*
 * Look for a process by specified pid.
 */
static track_proc_t *
proc_find_nolock(pid_t pid)
{
	track_proc_t *proc;
	int hashidx;

	/*
	 * To speed up, check the "latest access" process first.
	 */
	if ((s_proc_group.latest != NULL) &&
	    ((s_proc_group.latest)->pid == pid)) {
		proc = s_proc_group.latest;
		goto L_EXIT;
	}

	/*
	 * Scan the process hash table.
	 */
	hashidx = PROC_HASHTBL_INDEX(pid);
	proc = s_proc_group.hashtbl[hashidx];
	while (proc != NULL) {
		if (proc->pid == pid) {
			break;
		}

		proc = proc->hash_next;
	}

L_EXIT:
	if (proc != NULL) {
		if (proc_refcount_inc(proc) != 0) {
			/*
			 * The proc is tagged as removing.
			 */
			if (s_proc_group.latest == proc) {
				s_proc_group.latest = NULL;
			}

			proc = NULL;
		}
	}

	if (proc != NULL) {
		s_proc_group.latest = proc;
	}

	return (proc);
}

/*
 * Look for a process by pid with lock protection.
 */
track_proc_t *
proc_find(pid_t pid)
{
	track_proc_t *proc;

	(void) pthread_mutex_lock(&s_proc_group.mutex);
	proc = proc_find_nolock(pid);
	(void) pthread_mutex_unlock(&s_proc_group.mutex);
	return (proc);
}

/*
 * Allocation and initialization for a new 'track_proc_t' structure.
 */
static track_proc_t *
proc_alloc(void)
{
	int cpuid_max;
	track_proc_t *proc;
	count_value_t *countval_arr;

	if ((cpuid_max = node_cpuid_max()) <= 0) {
		return (NULL);
	}

	if ((countval_arr = zalloc(cpuid_max *
	    sizeof (count_value_t))) == NULL) {
		return (NULL);
	}

	if ((proc = zalloc(sizeof (track_proc_t))) == NULL) {
		free(countval_arr);
		return (NULL);
	}

	if (pthread_mutex_init(&proc->mutex, NULL) != 0) {
		free(countval_arr);
		free(proc);
		return (NULL);
	}

	proc->pid = -1;
	proc->countval_arr = countval_arr;
	proc->cpuid_max = cpuid_max;
	os_pqos_cmt_init(&proc->pqos);
	proc->inited = B_TRUE;
	return (proc);
}

static int
lwp_id_cmp(const void *a, const void *b)
{
	const track_lwp_t *lwp1 = (const track_lwp_t *)a;
	const track_lwp_t *lwp2 = *((track_lwp_t *const *)b);

	if (lwp1->id > lwp2->id) {
		return (1);
	}

	if (lwp1->id < lwp2->id) {
		return (-1);
	}

	return (0);
}

/*
 * Look for a thread in 'lwp_list' of proc.
 */
track_lwp_t *
proc_lwp_find(track_proc_t *proc, id_t lwpid)
{
	track_lwp_t *lwp = NULL, **p, lwp_key;
	proc_lwplist_t *list = &proc->lwp_list;

	lwp_key.id = lwpid;
	(void) pthread_mutex_lock(&proc->mutex);
	if ((p = bsearch(&lwp_key, (void *)(list->id_arr),
	    list->nlwps, sizeof (track_lwp_t *),
	    lwp_id_cmp)) != NULL) {

		lwp = *p;
		if (lwp_refcount_inc(lwp) != 0) {
			/*
			 * The lwp is being removed by other threads or
			 * the thread is quitting.
			 */
			lwp = NULL;
		}
	}

	(void) pthread_mutex_unlock(&proc->mutex);
	return (lwp);
}

static int
lwp_key_cmp(const void *a, const void *b)
{
	const track_lwp_t *lwp1 = *((track_lwp_t *const *)a);
	const track_lwp_t *lwp2 = *((track_lwp_t *const *)b);

	if (lwp1->key > lwp2->key) {
		return (-1);
	}

	if (lwp1->key < lwp2->key) {
		return (1);
	}

	return (0);
}

static void
proc_lwp_sortkey(track_proc_t *proc)
{
	proc_lwplist_t *list = &proc->lwp_list;
	track_lwp_t **sort_arr;

	if (list->sort_arr != NULL) {
		free(list->sort_arr);
		list->sort_arr = NULL;
	}

	if ((sort_arr = zalloc(sizeof (track_lwp_t *) * list->nlwps)) == NULL) {
		return;
	}

	(void) memcpy(sort_arr, list->id_arr,
	    sizeof (track_lwp_t *) * list->nlwps);
	qsort(sort_arr, list->nlwps, sizeof (track_lwp_t *), lwp_key_cmp);
	list->sort_arr = sort_arr;
	list->sort_idx = 0;
}

/*
 * Sort the threads in a specified process.
 */
void
proc_lwp_resort(track_proc_t *proc, sort_key_t sort)
{
	/*
	 * The lock "proc->mutex" takes outside.
	 */
	proc_lwp_traverse(proc, lwp_key_compute, &sort);
	proc_lwp_sortkey(proc);
}

/*
 * Count the total number of processes and threads.
 */
void
proc_lwp_count(int *nprocs, int *nlwps)
{
	if (nprocs != NULL) {
		*nprocs = s_proc_group.nprocs;
	}

	if (nlwps != NULL) {
		*nlwps = s_proc_group.nlwps;
	}
}

void
proc_group_lock(void)
{
	(void) pthread_mutex_lock(&s_proc_group.mutex);
}

void
proc_group_unlock(void)
{
	(void) pthread_mutex_unlock(&s_proc_group.mutex);
}

static uint64_t
count_value_get(track_proc_t *proc, ui_count_id_t ui_count_id)
{
	return (node_countval_sum(proc->countval_arr, proc->cpuid_max,
	    NODE_ALL, ui_count_id));
}

/*
 * Compute the value of key for process sorting.
 */
/* ARGSUSED */
static int
proc_key_compute(track_proc_t *proc, void *arg, boolean_t *end)
{
	sort_key_t sortkey = *((sort_key_t *)arg);
	uint64_t rma, lma, clk, ir;

	switch (sortkey) {
	case SORT_KEY_CPU:
		proc->key = count_value_get(proc, UI_COUNT_CLK);
		break;

	case SORT_KEY_PID:
		proc->key = proc->pid;
		break;

	case SORT_KEY_RPI:
		rma = count_value_get(proc, UI_COUNT_RMA);
		ir = count_value_get(proc, UI_COUNT_IR);
		proc->key = (uint64_t)ratio(rma * 1000, ir);
		break;

	case SORT_KEY_LPI:
		lma = count_value_get(proc, UI_COUNT_LMA);
		ir = count_value_get(proc, UI_COUNT_IR);
		proc->key = (uint64_t)ratio(lma * 1000, ir);
		break;

	case SORT_KEY_CPI:
		clk = count_value_get(proc, UI_COUNT_CLK);
		ir = count_value_get(proc, UI_COUNT_IR);
		proc->key = (uint64_t)ratio(clk * 1000, ir);
		break;

	case SORT_KEY_RMA:
		proc->key = count_value_get(proc, UI_COUNT_RMA);
		break;

	case SORT_KEY_LMA:
		proc->key = count_value_get(proc, UI_COUNT_LMA);
		break;

	case SORT_KEY_RL:
		rma = count_value_get(proc, UI_COUNT_RMA);
		lma = count_value_get(proc, UI_COUNT_LMA);
		proc->key = (uint64_t)ratio(rma * 1000, lma);
		break;

	default:
		break;
	}

	*end = B_FALSE;
	return (0);
}

static int
proc_key_cmp(const void *a, const void *b)
{
	const track_proc_t *proc1 = *((track_proc_t *const *)a);
	const track_proc_t *proc2 = *((track_proc_t *const *)b);

	if (proc1->key > proc2->key) {
		return (-1);
	}

	if (proc1->key < proc2->key) {
		return (1);
	}

	return (0);
}

static int
proc_pid_cmp(const void *a, const void *b)
{
	const track_proc_t *proc1 = *((track_proc_t *const *)a);
	const track_proc_t *proc2 = *((track_proc_t *const *)b);

	if (proc1->pid > proc2->pid) {
		return (1);
	}

	if (proc1->pid < proc2->pid) {
		return (-1);
	}

	return (0);
}

static void
proc_sortkey(void)
{
	track_proc_t **sort_arr, *proc;
	int i, j = 0;

	if (s_proc_group.sort_arr != NULL) {
		free(s_proc_group.sort_arr);
		s_proc_group.sort_arr = NULL;
	}

	sort_arr = zalloc(sizeof (track_proc_t *) * s_proc_group.nprocs);
	if (sort_arr == NULL) {
		return;
	}

	for (i = 0; i < PROC_HASHTBL_SIZE; i++) {
		proc = s_proc_group.hashtbl[i];
		while (proc != NULL) {
			sort_arr[j++] = proc;
			proc = proc->hash_next;
		}

		if (j == s_proc_group.nprocs) {
			break;
		}
	}

	qsort(sort_arr, s_proc_group.nprocs,
	    sizeof (track_proc_t *), proc_pid_cmp);

	qsort(sort_arr, s_proc_group.nprocs,
	    sizeof (track_proc_t *), proc_key_cmp);

	s_proc_group.sort_arr = sort_arr;
	s_proc_group.sort_idx = 0;
}

/*
 * Resort the process by the value of key.
 */
void
proc_resort(sort_key_t sort)
{
	/*
	 * The lock of s_proc_group takes outside.
	 */
	proc_traverse(proc_key_compute, &sort);
	proc_sortkey();
}

/*
 * Move the 'sort_idx' to next proc node and return current one.
 */
track_proc_t *
proc_sort_next(void)
{
	int idx = s_proc_group.sort_idx;

	if (s_proc_group.sort_arr == NULL) {
		return (NULL);
	}

	if (idx < s_proc_group.nprocs) {
		s_proc_group.sort_idx++;
		return (s_proc_group.sort_arr[idx]);
	}

	return (NULL);
}

int
proc_nlwp(track_proc_t *proc)
{
	return (proc->lwp_list.nlwps);
}

/*
 * Add a new proc in s_process_group->hashtbl.
 */
static int
proc_group_add(track_proc_t *proc)
{
	track_proc_t *head;
	int hashidx;

	/*
	 * The lock of table has been taken outside.
	 */
	hashidx = PROC_HASHTBL_INDEX(proc->pid);
	if ((head = s_proc_group.hashtbl[hashidx]) != NULL) {
		head->hash_prev = proc;
	}

	proc->hash_next = head;
	proc->hash_prev = NULL;
	s_proc_group.hashtbl[hashidx] = proc;
	s_proc_group.nprocs++;
	return (0);
}

/*
 * Remove a specifiled proc from s_process_group->hashtbl.
 */
static void
proc_group_remove(track_proc_t *proc)
{
	track_proc_t *prev, *next;
	int hashidx;

	/*
	 * The lock of table has been taken outside.
	 */
	hashidx = PROC_HASHTBL_INDEX(proc->pid);

	/*
	 * Remove it from process hash-list.
	 */
	prev = proc->hash_prev;
	next = proc->hash_next;
	if (prev != NULL) {
		prev->hash_next = next;
	} else {
		s_proc_group.hashtbl[hashidx] = next;
	}

	if (next != NULL) {
		next->hash_prev = prev;
	}

	s_proc_group.nprocs--;
	if (s_proc_group.latest == proc) {
		s_proc_group.latest = NULL;
	}
}

/*
 * The process is not valid, remove it.
 */
static void
proc_obsolete(pid_t pid)
{
	track_proc_t *proc;

	if ((proc = proc_find(pid)) != NULL) {
		proc_refcount_dec(proc);
		(void) pthread_mutex_lock(&s_proc_group.mutex);
		proc_group_remove(proc);
		proc_free(proc);
		(void) pthread_mutex_unlock(&s_proc_group.mutex);
	}
}

static int
pid_cmp(const void *a, const void *b)
{
	const pid_t *pid1 = (const pid_t *)a;
	const pid_t *pid2 = (const pid_t *)b;

	if (*pid1 > *pid2) {
		return (1);
	}

	if (*pid1 < *pid2) {
		return (-1);
	}

	return (0);
}

static pid_t *
pid_find(pid_t pid, pid_t *pid_arr, int num)
{
	pid_t *p;

	p = bsearch(&pid, (void *)pid_arr, num, sizeof (pid_t), pid_cmp);
	return (p);
}

/* ARGSUSED */
static int
proc_lwp_refresh(track_proc_t *proc, void *arg, boolean_t *end)
{
	*end = B_FALSE;
	lwp_enum_update(proc);
	return (0);
}

/* ARGSUSED */
static int
proc_nlwps_sum(track_proc_t *proc, void *arg, boolean_t *end)
{
	*end = B_FALSE;
	s_proc_group.nlwps += proc_nlwp(proc);
	return (0);
}

/*
 * The array 'procs_new' contains the latest valid pid. Scan the hashtbl to
 * figure out the obsolete processes and remove them. For the new processes,
 * add them in hashtbl.
 */
static void
proc_group_refresh(pid_t *procs_new, int nproc_new)
{
	track_proc_t *proc, *hash_next;
	pid_t *p;
	int i, j;
	boolean_t *exist_arr;

	if ((exist_arr = zalloc(sizeof (boolean_t) * nproc_new)) == NULL) {
		return;
	}

	qsort(procs_new, nproc_new, sizeof (pid_t), pid_cmp);

	(void) pthread_mutex_lock(&s_proc_group.mutex);
	for (i = 0; i < PROC_HASHTBL_SIZE; i++) {
		proc = s_proc_group.hashtbl[i];
		while (proc != NULL) {
			hash_next = proc->hash_next;
			if ((p = pid_find(proc->pid, procs_new,
			    nproc_new)) == NULL) {
				proc_group_remove(proc);
				proc_free(proc);
			} else {
				j = ((uint64_t)p - (uint64_t)procs_new) /
				    sizeof (pid_t);
				exist_arr[j] = B_TRUE;
			}

			proc = hash_next;
		}
	}

	for (i = 0; i < nproc_new; i++) {
		if (!exist_arr[i]) {
			if ((proc = proc_alloc()) != NULL) {
				proc->pid = procs_new[i];
				(void) os_procfs_pname_get(proc->pid,
				    proc->name, PROC_NAME_SIZE);
				(void) proc_group_add(proc);
			}
		}
	}

	s_proc_group.nprocs = nproc_new;
	s_proc_group.nlwps = 0;
	proc_traverse(proc_lwp_refresh, NULL);
	proc_traverse(proc_nlwps_sum, NULL);
	(void) pthread_mutex_unlock(&s_proc_group.mutex);
	free(exist_arr);
}

/*
 * Update the valid processes by scanning '/proc'
 */
void
proc_enum_update(pid_t pid)
{
	pid_t *procs_new;
	int nproc_new;

	if (pid > 0) {
		if (kill(pid, 0) == -1) {
			/* The process is obsolete. */
			proc_obsolete(pid);
		}
	} else {
		if (procfs_proc_enum(&procs_new, &nproc_new) == 0) {
			proc_group_refresh(procs_new, nproc_new);
			free(procs_new);
		}
	}
}

/*
 * Increment for the refcount.
 */
int
proc_refcount_inc(track_proc_t *proc)
{
	int ret = -1;

	(void) pthread_mutex_lock(&proc->mutex);
	if (!proc->removing) {
		proc->ref_count++;
		ret = 0;
	}

	(void) pthread_mutex_unlock(&proc->mutex);
	return (ret);
}

/*
 * Decrement for the refcount. If the refcount turns to be 0 and the
 * 'removing' flag is set, release the 'track_proc_t' structure.
 */
void
proc_refcount_dec(track_proc_t *proc)
{
	boolean_t remove = B_FALSE;

	(void) pthread_mutex_lock(&proc->mutex);
	proc->ref_count--;
	if ((proc->ref_count == 0) && (proc->removing)) {
		remove = B_TRUE;
	}
	(void) pthread_mutex_unlock(&proc->mutex);

	if (remove) {
		(void) pthread_mutex_lock(&s_proc_group.mutex);
		proc_free(proc);
		(void) pthread_mutex_unlock(&s_proc_group.mutex);
	}
}

/*
 * Walk through all the threads in process and call 'func()'
 * for each thread.
 */
void
proc_lwp_traverse(track_proc_t *proc,
	int (*func)(track_lwp_t *, void *, boolean_t *), void *arg)
{
	track_lwp_t *lwp;
	proc_lwplist_t *list = &proc->lwp_list;
	boolean_t end;
	int i;

	/*
	 * The mutex "proc->mutex" has been taken outside.
	 */
	if (list->id_arr == NULL) {
		return;
	}

	for (i = 0; i < list->nlwps; i++) {
		if ((lwp = list->id_arr[i]) != NULL) {
			func(lwp, arg, &end);
			if (end) {
				break;
			}
		}
	}
}

/*
 * Update the process's per CPU perf data.
 */
int
proc_countval_update(track_proc_t *proc, int cpu, perf_count_id_t perf_count_id,
    uint64_t value)
{
	count_value_t *countval, *arr_new;
	int cpuid_max = node_cpuid_max();

	/*
	 * Check if new cpu hotadd/online
	 */
	if (cpu >= proc->cpuid_max) {
		ASSERT(cpuid_max > proc->cpuid_max);
		if ((arr_new = realloc(proc->countval_arr,
		    sizeof (count_value_t) * cpuid_max)) == NULL) {
			return (-1);
		}

		(void) memset(&arr_new[proc->cpuid_max], 0,
		    sizeof (count_value_t) * (cpuid_max - proc->cpuid_max));

		proc->countval_arr = arr_new;
		proc->cpuid_max = cpuid_max;
	}

	countval = &proc->countval_arr[cpu];
	countval->counts[perf_count_id] += value;
	return (0);
}

static int
intval_update(track_proc_t *proc, void *arg, boolean_t *end)
{
	int intval_ms = *((int *)arg);

	*end = B_FALSE;
	proc->intval_ms = intval_ms;
	lwp_intval_update(proc, intval_ms);
	return (0);
}

/*
 * Update with the interval of sampling.
 */
void
proc_intval_update(int intval_ms)
{
	(void) pthread_mutex_lock(&s_proc_group.mutex);
	proc_traverse(intval_update, &intval_ms);
	(void) pthread_mutex_unlock(&s_proc_group.mutex);
}

int
proc_intval_get(track_proc_t *proc)
{
	return (proc->intval_ms);
}

/* ARGSUSED */
static int
lwp_profiling_clear(track_lwp_t *lwp, void *arg, boolean_t *end)
{
	*end = B_FALSE;
	(void) memset(lwp->countval_arr, 0,
	    sizeof (count_value_t) * lwp->cpuid_max);
	return (0);
}

/* ARGSUSED */
static int
profiling_clear(track_proc_t *proc, void *arg, boolean_t *end)
{
	*end = B_FALSE;
	proc_lwp_traverse(proc, lwp_profiling_clear, NULL);
	(void) memset(proc->countval_arr, 0,
	    sizeof (count_value_t) * proc->cpuid_max);
	return (0);
}

void
proc_profiling_clear(void)
{
	proc_traverse(profiling_clear, NULL);
}

/* ARGSUSED */
static int
lwp_callchain_clear(track_lwp_t *lwp, void *arg, boolean_t *end)
{
	*end = B_FALSE;
	perf_countchain_reset(&lwp->count_chain);
	return (0);
}

/* ARGSUSED */
static int
callchain_clear(track_proc_t *proc, void *arg, boolean_t *end)
{
	*end = B_FALSE;
	proc_lwp_traverse(proc, lwp_callchain_clear, NULL);
	perf_countchain_reset(&proc->count_chain);
	return (0);
}

void
proc_callchain_clear(void)
{
	proc_traverse(callchain_clear, NULL);
}

/* ARGSUSED */
static int
lwp_ll_clear(track_lwp_t *lwp, void *arg, boolean_t *end)
{
	*end = B_FALSE;
	perf_llrecgrp_reset(&lwp->llrec_grp);
	return (0);
}

static int
ll_clear(track_proc_t *proc, void *arg, boolean_t *end)
{
        *end = B_FALSE;
        proc_lwp_traverse(proc, lwp_ll_clear, NULL);
        perf_llrecgrp_reset(&proc->llrec_grp);
        return (0);
}

void
proc_ll_clear(track_proc_t *proc)
{
	if (proc != NULL) {
		proc_lwp_traverse(proc, lwp_ll_clear, NULL);
		perf_llrecgrp_reset(&proc->llrec_grp);
	} else {
		proc_traverse(ll_clear, NULL);
	}
}

void proc_pqos_func(track_proc_t *proc,
	int (*func)(track_proc_t *, void *, boolean_t *))
{
	if (proc == NULL) {
		pthread_mutex_lock(&s_proc_group.mutex);
		proc_traverse(func, NULL);
		pthread_mutex_unlock(&s_proc_group.mutex);
	}
}
