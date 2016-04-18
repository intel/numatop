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

/* This file contains code to handle the 'tracked thread'. */

#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include "include/types.h"
#include "include/util.h"
#include "include/lwp.h"
#include "include/proc.h"
#include "include/perf.h"
#include "include/os/node.h"
#include "include/os/os_util.h"

/*
 * Allocation and initialization for a 'track_lwp_t' struture.
 * A 'track_lwp_t' structure is allocated for each tracked thread.
 */
static track_lwp_t *
lwp_alloc(void)
{
	int cpuid_max;
	track_lwp_t *lwp;
	count_value_t *countval_arr;
	boolean_t supported;

	if ((cpuid_max = node_cpuid_max()) <= 0) {
		return (NULL);
	}

	if ((countval_arr = zalloc(cpuid_max *
	    sizeof (count_value_t))) == NULL) {
		return (NULL);
	}

	if ((lwp = zalloc(sizeof (track_lwp_t))) == NULL) {
		free(countval_arr);
		return (NULL);
	}

	if (((lwp->perf_priv = perf_priv_alloc(&supported)) == NULL) &&
		(supported)) {
		free(countval_arr);
		free(lwp);
		return (NULL);
	}

	if (pthread_mutex_init(&lwp->mutex, NULL) != 0) {
		free(countval_arr);
		perf_priv_free(lwp->perf_priv);
		free(lwp);
		return (NULL);
	}

	lwp->countval_arr = countval_arr;
	lwp->cpuid_max = cpuid_max;
	os_pqos_cmt_init(&lwp->pqos);
	lwp->inited = B_TRUE;
	return (lwp);
}

/*
 * Clean up the resource of 'track_lwp_t' struture. Before calling
 * lwp_free(), 'track_lwp_t' must be removed from 'lwp_list' in
 * 'track_proc_t' structure.
 */
int
lwp_free(track_lwp_t *lwp)
{
	(void) pthread_mutex_lock(&lwp->mutex);
	if (lwp->ref_count > 0) {
		lwp->removing = B_TRUE;
		(void) pthread_mutex_unlock(&lwp->mutex);
		return (-1);
	}

	if (lwp->countval_arr != NULL) {
		free(lwp->countval_arr);
	}

	perf_priv_free(lwp->perf_priv);
	perf_countchain_reset(&lwp->count_chain);
	perf_llrecgrp_reset(&lwp->llrec_grp);

	os_perf_pqos_free(&lwp->pqos);

	(void) pthread_mutex_unlock(&lwp->mutex);
	(void) pthread_mutex_destroy(&lwp->mutex);

	free(lwp);
	return (0);
}

/*
 * Move the 'sort_idx' to next lwp node and return current one.
 */
track_lwp_t *
lwp_sort_next(track_proc_t *proc)
{
	proc_lwplist_t *list = &proc->lwp_list;
	int idx = list->sort_idx;

	if (list->sort_arr == NULL) {
		return (NULL);
	}

	if (idx < list->nlwps) {
		list->sort_idx++;
		return (list->sort_arr[idx]);
	}

	return (NULL);
}

static int
id_cmp(const void *a, const void *b)
{
	int *id1 = (int *)a;
	int *id2 = (int *)b;

	if (*id1 > *id2) {
		return (1);
	}

	if (*id1 < *id2) {
		return (-1);
	}

	return (0);
}

/*
 * Enumerate valid threads from '/proc', remove the obsolete threads.
 */
void
lwp_enum_update(track_proc_t *proc)
{
	proc_lwplist_t *list = &proc->lwp_list;
	track_lwp_t *lwp;
	track_lwp_t **arr_new, **arr_old;
	int *lwps_new, nlwp_new;
	int i = 0, j = 0, k;

	if (os_procfs_lwp_enum(proc->pid, &lwps_new, &nlwp_new) != 0) {
		return;
	}

	qsort(lwps_new, nlwp_new, sizeof (int), id_cmp);

	if ((arr_new = zalloc(sizeof (track_lwp_t *) * nlwp_new)) == NULL) {
		goto L_EXIT;
	}

	(void) pthread_mutex_lock(&proc->mutex);

	if ((arr_old = list->id_arr) != NULL) {
		while ((i < nlwp_new) && (j < list->nlwps)) {
			if (lwps_new[i] == arr_old[j]->id) {
				arr_new[i] = arr_old[j];
				i++;
				j++;
				continue;
			}

			if (lwps_new[i] < arr_old[j]->id) {
				if ((lwp = lwp_alloc()) != NULL) {
					lwp->id = lwps_new[i];
					lwp->proc = proc;
					arr_new[i] = lwp;
				}

				i++;
				continue;
			}

			/* The lwpid (arr_old[j]->id) is obsolete */
			(void) lwp_free(arr_old[j]);
			j++;
		}
	}

	for (k = i; k < nlwp_new; k++) {
		if ((lwp = lwp_alloc()) != NULL) {
			lwp->id = lwps_new[k];
			lwp->proc = proc;
			arr_new[k] = lwp;
		}
	}

	if (arr_old != NULL) {
		for (k = j; k < list->nlwps; k++) {
			(void) lwp_free(arr_old[k]);
		}

		free(arr_old);
	}

	list->id_arr = arr_new;
	list->nlwps = nlwp_new;
	(void) pthread_mutex_unlock(&proc->mutex);

L_EXIT:
	free(lwps_new);
}

/*
 * lwp refcount increment. The 'track_lwp_t' structure can
 * be only released when the refcount is 0.
 */
int
lwp_refcount_inc(track_lwp_t *lwp)
{
	int ret = -1;

	(void) pthread_mutex_lock(&lwp->mutex);

	if ((!lwp->removing) && (!lwp->quitting)) {
		lwp->ref_count++;
		ret = 0;
	}

	(void) pthread_mutex_unlock(&lwp->mutex);
	return (ret);
}

/*
 * lwp refcount decrement. If the refcount is 0 after decrement
 * and the 'removing' flag is set, release the 'track_lwp_t' structure.
 */
void
lwp_refcount_dec(track_lwp_t *lwp)
{
	boolean_t remove = B_FALSE;

	(void) pthread_mutex_lock(&lwp->mutex);
	lwp->ref_count--;

	if ((lwp->ref_count == 0) && (lwp->removing)) {
		remove = B_TRUE;
	}

	(void) pthread_mutex_unlock(&lwp->mutex);
	if (remove) {
		(void) lwp_free(lwp);
	}
}

static uint64_t
count_value_get(track_lwp_t *lwp, count_id_t count_id)
{
	return (node_countval_sum(lwp->countval_arr, lwp->cpuid_max,
	    NODE_ALL, count_id));
}

/*
 * Compute the value of key for lwp sorting.
 */
int
lwp_key_compute(track_lwp_t *lwp, void *arg, boolean_t *end)
{
	sort_key_t sortkey = *((sort_key_t *)arg);

	switch (sortkey) {
	case SORT_KEY_CPU:
		lwp->key = count_value_get(lwp, COUNT_CLK);
		break;

	default:
		break;
	}

	*end = B_FALSE;
	return (0);
}

/*
 * Update the lwp's per CPU perf data.
 */
int
lwp_countval_update(track_lwp_t *lwp, int cpu, count_id_t count_id,
    uint64_t value)
{
	count_value_t *countval, *arr_new;
	int cpuid_max = node_cpuid_max();

	/*
	 * Check if new cpu hotadd/online
	 */
	if (cpu >= lwp->cpuid_max) {
		ASSERT(cpuid_max > lwp->cpuid_max);
		if ((arr_new = realloc(lwp->countval_arr,
		    sizeof (count_value_t) * cpuid_max)) == NULL) {
			return (-1);
		}

		(void) memset(&arr_new[lwp->cpuid_max], 0,
		    sizeof (count_value_t) * (cpuid_max - lwp->cpuid_max));

		lwp->countval_arr = arr_new;
		lwp->cpuid_max = cpuid_max;
	}

	countval = &lwp->countval_arr[cpu];
	countval->counts[count_id] += value;
	return (0);
}

int
lwp_intval_get(track_lwp_t *lwp)
{
	return (lwp->intval_ms);
}

static int
intval_update(track_lwp_t *lwp, void *arg, boolean_t *end)
{
	int intval_ms = *((int *)arg);

	*end = B_FALSE;
	lwp->intval_ms = intval_ms;
	return (0);
}

/*
 * Update with the sampling interval for all threads in process.
 */
void
lwp_intval_update(track_proc_t *proc, int intval_ms)
{
	(void) pthread_mutex_lock(&proc->mutex);
	proc_lwp_traverse(proc, intval_update, &intval_ms);
	(void) pthread_mutex_unlock(&proc->mutex);
}

void
lwp_quitting_set(track_lwp_t *lwp)
{
	lwp->quitting = B_TRUE;
}
