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

/* This file contains code to handle the node. */

#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include "../include/types.h"
#include "../include/util.h"
#include "../include/os/os_util.h"
#include "../include/os/pfwrapper.h"
#include "../include/os/node.h"

static node_group_t s_node_group;

static void
node_init(node_t *node, int nid, boolean_t hotadd)
{
	memset(node, 0, sizeof (node_t));
	os_perf_cpuarr_init(node->cpus, NCPUS_NODE_MAX, hotadd);
	node->nid = nid;
	node->hotadd = hotadd;
}

static void
node_fini(node_t *node)
{
	os_perf_cpuarr_fini(node->cpus, NCPUS_NODE_MAX, B_FALSE);
	node->ncpus = 0;
	node->nid = INVALID_NID;
}

static void
node_hotremove(node_t *node)
{
	node->hotremove = B_TRUE;
	os_perf_cpuarr_fini(node->cpus, NCPUS_NODE_MAX, B_TRUE);
}

/*
 * Initialization for the node group.
 */
int
node_group_init(void)
{
	int i;
	node_t *node;

	(void) memset(&s_node_group, 0, sizeof (node_group_t));
	if (pthread_mutex_init(&s_node_group.mutex, NULL) != 0) {
		return (-1);
	}

	s_node_group.inited = B_TRUE;
	for (i = 0; i < NNODES_MAX; i++) {
		node = node_get(i);
		node_init(node, INVALID_NID, B_FALSE);
	}

	return (node_group_refresh(B_TRUE));
}

/*
 * Free the resources of all nodes in group.
 */
static void
node_group_reset(void)
{
	node_t *node;
	int i;

	for (i = 0; i < NNODES_MAX; i++) {
		node = node_get(i);
		node_fini(node);
	}

	s_node_group.nnodes = 0;
}

/*
 * Clean up all the resources of node group.
 */
void
node_group_fini(void)
{
	if (s_node_group.inited) {
		(void) pthread_mutex_destroy(&s_node_group.mutex);
		node_group_reset();
	}
}

static int
nid_find(int nid, int *arr, int num)
{
	int i;

	if (nid == INVALID_NID) {
		return (-1);
	}

	for (i = 0; i < num; i++) {
		if (arr[i] == nid) {
			return (i);
		}
	}

	return (-1);
}

static int
cpuid_max_get(int *cpu_arr, int num)
{
	int i, cpuid_max = -1;
	
	for (i = 0; i < num; i++) {
		if (cpuid_max < cpu_arr[i]) {
			cpuid_max = cpu_arr[i];
		}	
	}
	
	return (cpuid_max);
}

static int
cpu_refresh(boolean_t init)
{
	int i, j, num, cpuid_max = -1;
	int cpu_arr[NCPUS_NODE_MAX];
	node_t *node;

	for (i = 0; i < NNODES_MAX; i++) {
		node = node_get(i);
		if (NODE_VALID(node)) {
			if (!os_sysfs_cpu_enum(node->nid, cpu_arr, NCPUS_NODE_MAX, &num)) {
				return (-1);
			}

			if (os_perf_cpuarr_refresh(node->cpus, NCPUS_NODE_MAX, cpu_arr,
				num, init) != 0) {
				return (-1);
			}

			node->ncpus = num;
			j = cpuid_max_get(cpu_arr, num);
			if (cpuid_max < j) {
				cpuid_max = j;
			}
		}
	}

	if (cpuid_max > s_node_group.cpuid_max) {
		s_node_group.cpuid_max = cpuid_max;
	}

	/* Refresh the number of online CPUs */
	g_ncpus = os_sysfs_online_ncpus();
	return (0);
}

static int
meminfo_refresh(void)
{
	int i;
	node_t *node;
	
	for (i = 0; i < NNODES_MAX; i++) {
		node = node_get(i);
		if (NODE_VALID(node)) {
			if (!os_sysfs_meminfo(node->nid, &node->meminfo)) {
				debug_print(NULL, 2, "meminfo_refresh:sysfs_meminfo failed\n");
				return (-1);
			}
		}
	}

	return (0);
}

/*
 * Refresh the information of each node in group. The information
 * includes such as: CPU, physical memory size, free memory size.
 * Get the information from /sys/devices/system/node.
 */
int
node_group_refresh(boolean_t init)
{
	int node_arr[NNODES_MAX];
	int num, i, j, ret = -1;
	node_t *node;

	node_group_lock();
	if (!os_sysfs_node_enum(node_arr, NNODES_MAX, &num)) {
		goto L_EXIT;
	}

	for (i = 0; i < NNODES_MAX; i++) {
		node = node_get(i);
		if (NODE_VALID(node)) {
			if ((j = nid_find(node->nid, node_arr, num)) == -1) {
				node_hotremove(node);
				s_node_group.nnodes--;
			} else {
				node_arr[j] = INVALID_NID;
			}
		}
	}

	for (i = 0; i < NNODES_MAX; i++) {
		node = node_get(i);
		if (!NODE_VALID(node)) {
			if ((j = nid_find(i, node_arr, num)) >= 0) {
				ASSERT(node_arr[j] == i);
				if (init) {
					node_init(node, i, B_FALSE);
				} else {				
					node_init(node, i, B_TRUE);
				}

				s_node_group.nnodes++;
				node_arr[j] = INVALID_NID;
			}
		}
	}

	if (cpu_refresh(init) != 0) {
		goto L_EXIT;
	}

	if (meminfo_refresh() != 0) {
		goto L_EXIT;
	}

	ret = 0;

L_EXIT:
	node_group_unlock();
	return (ret);
}

node_t *
node_get(int nid)
{
	return (&s_node_group.nodes[nid]);
}

int
node_num(void)
{
	return (s_node_group.nnodes);
}

void
node_group_lock(void)
{
	(void) pthread_mutex_lock(&s_node_group.mutex);
}

void
node_group_unlock(void)
{
	(void) pthread_mutex_unlock(&s_node_group.mutex);
}

/*
 * Get node by CPU id.
 */
node_t *
node_by_cpu(int cpuid)
{
	node_t *node;
	int i, j;

	if (cpuid == INVALID_CPUID) {
		return (NULL);
	}

	for (i = 0; i < NNODES_MAX; i++) {
		node = node_get(i);
		if (!NODE_VALID(node)) {
			continue;
		}

		for (j = 0; j < NCPUS_NODE_MAX; j++) {
			if (cpuid == node->cpus[j].cpuid) {
				return (node);
			}
		}
	}

	return (NULL);
}

int
node_ncpus(node_t *node)
{
	return (node->ncpus);
}

int
node_intval_get(void)
{
	return (s_node_group.intval_ms);
}

/*
 * Update the node's perf data.
 */
void
node_countval_update(node_t *node, count_id_t count_id, uint64_t value)
{
	node->countval.counts[count_id] += value;
}

/*
 * Return the perf data of specified node and count.
 */
uint64_t
node_countval_get(node_t *node, count_id_t count_id)
{
	return (node->countval.counts[count_id]);
}

/*
 * Return the memory information for a specified node.
 */
void
node_meminfo(int nid, node_meminfo_t *info)
{
	node_t *node;
	
	if (((node = node_get(nid)) != NULL) &&
		(NODE_VALID(node))) {
		memcpy(info, &node->meminfo, sizeof (node_meminfo_t));	
	}
}

/*
 * Walk through the CPUs in a node and call 'func()' for each CPU.
 * The CPU might be INVALID. Note that the function could be only called
 * in perf thread.
 */
int
node_cpu_traverse(pfn_perf_cpu_op_t func, void *arg, boolean_t err_ret,
	pfn_perf_cpu_op_t hotadd_func)
{
	node_t *node;
	perf_cpu_t *cpu;
	int i, j, ret;

	for (i = 0; i < NNODES_MAX; i++) {
		node = node_get(i);
		if (!NODE_VALID(node)) {
			continue;
		}

		for (j = 0; j < NCPUS_NODE_MAX; j++) {
			cpu = &node->cpus[j];
			if (cpu->hotremove) {
				pf_resource_free(cpu);
				cpu->hotremove = B_FALSE;
				cpu->cpuid = INVALID_CPUID;
				continue;
			}

			if ((cpu->hotadd) && (hotadd_func != NULL)) {
				hotadd_func(cpu, arg);
				cpu->hotadd = B_FALSE;
			}

			if ((func != NULL) && (cpu->cpuid != INVALID_CPUID) && (!cpu->hotadd)) {
				if (((ret = func(cpu, arg)) != 0) && (err_ret)) {
					return (ret);					
				} 
			}
		}

		if (node->hotremove) {
			node->nid = INVALID_NID;
			node->hotremove = B_FALSE;
		}
	}

	return (0);
}

uint64_t
countval_sum(count_value_t *countval_arr, int cpuid_max, int nid,
	count_id_t count_id)
{
	uint64_t value = 0;
	node_t *node;
	int i, cpuid, num = 0;

	node = node_get(nid);
	if (!NODE_VALID(node)) {
		return (0);
	}

	for (i = 0; i < NCPUS_NODE_MAX; i++) {
		if (num >= node->ncpus) {
			break;
		}

		if ((cpuid = node->cpus[i].cpuid) != INVALID_CPUID) {
			value += countval_arr[cpuid].counts[count_id];
			num++;
		}
	}

	return (value);
}

uint64_t
node_countval_sum(count_value_t *countval_arr, int cpuid_max, int nid,
	count_id_t count_id)
{
	int i;
	uint64_t value = 0;

	if (nid != NODE_ALL) {
		return (countval_sum(countval_arr, cpuid_max, nid, count_id));
	}

	for (i = 0; i < NNODES_MAX; i++) {
		value += countval_sum(countval_arr, cpuid_max, i, count_id);
	}

	return (value);
}

perf_cpu_t *
node_cpus(node_t *node)
{
	return (node->cpus);
}

void
node_intval_update(int intval_ms)
{
	s_node_group.intval_ms = intval_ms;
}

void
node_profiling_clear(void)
{
	node_t *node;
	int i;

	for (i = 0; i < NNODES_MAX; i++) {
		node = node_get(i);
		(void) memset(&node->countval, 0, sizeof (count_value_t));
	}	
}

node_t *
node_valid_get(int node_idx)
{
	int i, nvalid = 0;
	node_t *node;

	for (i = 0; i < NNODES_MAX; i++) {
		node = node_get(i);
		if (NODE_VALID(node)) {
			if (node_idx == nvalid) {
				return (node);	
			}
			
			nvalid++;
		}
	}

	return (NULL);
}

int
node_cpuid_max(void)
{
	return (s_node_group.cpuid_max + 1);
}

int
node_qpi_init(void)
{
	qpi_info_t qpi_tmp[NODE_QPI_MAX];
	int qpi_num, i;
	node_t *node;

	qpi_num = os_sysfs_uncore_qpi_init(qpi_tmp, NODE_QPI_MAX);
	if (qpi_num < 0)
		return -1;

	node_group_lock();

	for (i = 0; i < NNODES_MAX; i++) {
		node = node_get(i);
		if (NODE_VALID(node) && (qpi_num > 0)) {
			memcpy(node->qpi.qpi_info, qpi_tmp,
				sizeof(node->qpi.qpi_info));
			node->qpi.qpi_num = qpi_num;
		}
	}

	node_group_unlock();

	debug_print(NULL, 2, "%d QPI links per node\n", qpi_num);

	return 0;
}
