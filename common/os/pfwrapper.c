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

/* This file contains code to process the syscall "perf_event_open" */

#include <inttypes.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include "../include/types.h"
#include "../include/perf.h"
#include "../include/util.h"
#include "../include/os/pfwrapper.h"
#include "../include/os/node.h"
#include "../include/os/os_perf.h"

static int s_mapsize, s_mapmask;

static int
pf_event_open(struct perf_event_attr *attr, pid_t pid, int cpu, int group_fd,
	unsigned long flags)
{
	return (syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags));
}

static int
mmap_buffer_read(struct perf_event_mmap_page *header, void *buf, size_t size)
{
	void *data;
	uint64_t data_head, data_tail;
	int data_size, ncopies;
	
	/*
	 * The first page is a meta-data page (struct perf_event_mmap_page),
	 * so move to the second page which contains the perf data.
	 */
	data = (void *)header + g_pagesize;

	/*
	 * data_tail points to the position where userspace last read,
	 * data_head points to the position where kernel last add.
	 * After read data_head value, need to issue a rmb(). 
	 */
	data_tail = header->data_tail;
	data_head = header->data_head;
	rmb();

	/*
	 * The kernel function "perf_output_space()" guarantees no data_head can
	 * wrap over the data_tail.
	 */
	if ((data_size = data_head - data_tail) < size) {
		return (-1);
	}

	data_tail &= s_mapmask;

	/*
	 * Need to consider if data_head is wrapped when copy data.
	 */
	if ((ncopies = (s_mapsize - data_tail)) < size) {
		memcpy(buf, data + data_tail, ncopies);
		memcpy(buf + ncopies, data, size - ncopies);
	} else {
		memcpy(buf, data + data_tail, size);
	}

	header->data_tail += size;	
	return (0);
}

static void
mmap_buffer_skip(struct perf_event_mmap_page *header, int size)
{
	int data_head;

	data_head = header->data_head;
	rmb();

	if ((header->data_tail + size) > data_head) {
		size = data_head - header->data_tail;
	}

	header->data_tail += size;
}

static void
mmap_buffer_reset(struct perf_event_mmap_page *header)
{
	int data_head;

	data_head = header->data_head;;
	rmb();

	header->data_tail = data_head;
}

int
pf_ringsize_init(void)
{
	switch (g_precise) {
	case PRECISE_HIGH:
		s_mapsize = g_pagesize * (PF_MAP_NPAGES_MAX + 1);
		s_mapmask = (g_pagesize * PF_MAP_NPAGES_MAX) - 1;
		break;
		
	case PRECISE_LOW:
		s_mapsize = g_pagesize * (PF_MAP_NPAGES_MIN + 1);
		s_mapmask = (g_pagesize * PF_MAP_NPAGES_MIN) - 1;
		break;

	default:
		s_mapsize = g_pagesize * (PF_MAP_NPAGES_NORMAL + 1);
		s_mapmask = (g_pagesize * PF_MAP_NPAGES_NORMAL) - 1;
		break;	
	}

	return (s_mapsize - g_pagesize);
}

int
pf_profiling_setup(struct _perf_cpu *cpu, int idx, pf_conf_t *conf)
{
	struct perf_event_attr attr;
	int *fds = cpu->fds;
	int group_fd;

	memset(&attr, 0, sizeof (attr));
	attr.type = conf->type;	
	attr.config = conf->config;
	attr.config1 = conf->config1;
	attr.sample_period = conf->sample_period;
	attr.sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_READ |
		PERF_SAMPLE_CALLCHAIN;
	attr.read_format = PERF_FORMAT_GROUP |
		PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
	attr.size = sizeof(attr);

	debug_print(NULL, 2, "pf_profiling_setup: attr.type = 0x%lx, "
		"attr.config = 0x%lx, attr.config1 = 0x%lx\n",
		 attr.type, attr.config, attr.config1);

	if (idx == 0) {
		attr.disabled = 1;
		group_fd = -1;
	} else {
		group_fd = fds[0];;
	}

	if ((fds[idx] = pf_event_open(&attr, -1, cpu->cpuid, group_fd, 0)) < 0) {
		debug_print(NULL, 2, "pf_profiling_setup: pf_event_open is failed "
			"for CPU%d, COUNT%d\n", cpu->cpuid, idx);
		fds[idx] = INVALID_FD;
		return (-1);
	}
	
	if (idx == 0) {
		if ((cpu->map_base = mmap(NULL, s_mapsize, PROT_READ | PROT_WRITE,
			MAP_SHARED, fds[0], 0)) == MAP_FAILED) {
			close(fds[0]);
			fds[0] = INVALID_FD;
			return (-1);	
		}

		cpu->map_len = s_mapsize;
		cpu->map_mask = s_mapmask;
	} else {
	        if (ioctl(fds[idx], PERF_EVENT_IOC_SET_OUTPUT, fds[0]) != 0) {
			debug_print(NULL, 2, "pf_profiling_setup: "
				"PERF_EVENT_IOC_SET_OUTPUT is failed for CPU%d, COUNT%d\n",
				cpu->cpuid, idx);
			close(fds[idx]);
			fds[idx] = INVALID_FD;
			return (-1);
		}
	}

	return (0);
}

int
pf_profiling_start(struct _perf_cpu *cpu, perf_count_id_t perf_count_id)
{
	if (cpu->fds[perf_count_id] != INVALID_FD) {
		return (ioctl(cpu->fds[perf_count_id], PERF_EVENT_IOC_ENABLE, 0));
	}
	
	return (0);
}

int
pf_profiling_stop(struct _perf_cpu *cpu, perf_count_id_t perf_count_id)
{
	if (cpu->fds[perf_count_id] != INVALID_FD) {
		return (ioctl(cpu->fds[perf_count_id], PERF_EVENT_IOC_DISABLE, 0));
	}
	
	return (0);
}

int
pf_profiling_allstart(struct _perf_cpu *cpu)
{
	return (pf_profiling_start(cpu, 0));
}

int
pf_profiling_allstop(struct _perf_cpu *cpu)
{
	return (pf_profiling_stop(cpu, 0));
}

static uint64_t
scale(uint64_t value, uint64_t time_enabled, uint64_t time_running)
{
	uint64_t res = 0;

	if (time_running > time_enabled) {
		debug_print(NULL, 2, "time_running > time_enabled\n");
	}

	if (time_running) {
		res = (uint64_t)((double)value * (double)time_enabled / (double)time_running);
	}

	return (res);
}

static int
profiling_sample_read(struct perf_event_mmap_page *mhdr, int size,
	pf_profiling_rec_t *rec)
{
	struct { uint32_t pid, tid; } id;
	count_value_t *countval = &rec->countval;
	uint64_t i, time_enabled, time_running, nr, value, *ips;
	int j, ret = -1;

	/*
	 * struct read_format {
	 *	{ u32	pid, tid; }
	 *	{ u64	nr; }
	 *	{ u64	time_enabled; }
	 *	{ u64	time_running; }
	 *	{ u64	cntr[nr]; }
	 *	[ u64	nr; }
	 *	{ u64   ips[nr]; }
	 * };
	 */
	if (mmap_buffer_read(mhdr, &id, sizeof (id)) == -1) {
		debug_print(NULL, 2, "profiling_sample_read: read pid/tid failed.\n");
		goto L_EXIT;
	}

	size -= sizeof (id);

	if (mmap_buffer_read(mhdr, &nr, sizeof (nr)) == -1) {
		debug_print(NULL, 2, "profiling_sample_read: read nr failed.\n");
		goto L_EXIT;
	}

	size -= sizeof (nr);

	if (mmap_buffer_read(mhdr, &time_enabled, sizeof (time_enabled)) == -1) {
		debug_print(NULL, 2, "profiling_sample_read: read time_enabled failed.\n");
		goto L_EXIT;
	}

	size -= sizeof (time_enabled);

	if (mmap_buffer_read(mhdr, &time_running, sizeof (time_running)) == -1) {
		debug_print(NULL, 2, "profiling_sample_read: read time_running failed.\n");
		goto L_EXIT;
	}

	size -= sizeof (time_running);

	for (i = 0; i < nr; i++) {
		if (mmap_buffer_read(mhdr, &value, sizeof (value)) == -1) {
			debug_print(NULL, 2, "profiling_sample_read: read value failed.\n");
			goto L_EXIT;
		}

		size -= sizeof (value);

		/*
		 * Prevent the inconsistent results if share the PMU with other users
		 * who multiplex globally.
		 */
		value = scale(value, time_enabled, time_running);
		countval->counts[i] = value;
	}

	if (mmap_buffer_read(mhdr, &nr, sizeof (nr)) == -1) {
		debug_print(NULL, 2, "profiling_sample_read: read nr failed.\n");
		goto L_EXIT;
	}

	size -= sizeof (nr);

	j = 0;
	ips = rec->ips;
	for (i = 0; i < nr; i++) {
		if (j >= IP_NUM) {
			break;
		}

		if (mmap_buffer_read(mhdr, &value, sizeof (value)) == -1) {
			debug_print(NULL, 2, "profiling_sample_read: read value failed.\n");
			return (-1);
		}

		size -= sizeof (value);
		
		if (is_userspace(value)) {
			/*
			 * Only save the user-space address.
			 */
			ips[j] = value;
			j++;
		}
	}

	rec->ip_num = j;
	rec->pid = id.pid;
	rec->tid = id.tid;
	ret = 0;
	
L_EXIT:	
	if (size > 0) {
		mmap_buffer_skip(mhdr, size);
		debug_print(NULL, 2, "profiling_sample_read: skip %d bytes, ret=%d\n",
			size, ret);
	}

	return (ret);
}

static void
profiling_recbuf_update(pf_profiling_rec_t *rec_arr, int *nrec,
	pf_profiling_rec_t *rec)
{
	int i;

	if ((!nrec) || (rec->pid == 0) || (rec->tid == 0)) {
		/* Just consider the user-land process/thread. */
		return;	
	}

	/*
	 * The buffer of array is enough, don't need to consider overflow.
	 */
	i = *nrec;
	memcpy(&rec_arr[i], rec, sizeof (pf_profiling_rec_t));
	*nrec += 1;
}

void
pf_profiling_record(struct _perf_cpu *cpu, pf_profiling_rec_t *rec_arr,
	int *nrec)
{
	struct perf_event_mmap_page *mhdr = cpu->map_base;
	struct perf_event_header ehdr;
	pf_profiling_rec_t rec;
	int size;

	if (nrec != NULL) {
		*nrec = 0;
	}

	for (;;) {
		if (mmap_buffer_read(mhdr, &ehdr, sizeof(ehdr)) == -1) {
			return;
 		}

		if ((size = ehdr.size - sizeof (ehdr)) <= 0) {			
			mmap_buffer_reset(mhdr);
			return;
		}

		if ((ehdr.type == PERF_RECORD_SAMPLE) && (rec_arr != NULL)) {
			if (profiling_sample_read(mhdr, size, &rec) == 0) {
				profiling_recbuf_update(rec_arr, nrec, &rec);
			} else {
				/* No valid record in ring buffer. */
				return;	
			}
		} else {
			mmap_buffer_skip(mhdr, size);
		}
	}
}

int
pf_ll_setup(struct _perf_cpu *cpu, pf_conf_t *conf)
{
	struct perf_event_attr attr;
	int *fds = cpu->fds;

	memset(&attr, 0, sizeof (attr));
	attr.type = conf->type;
	attr.config = conf->config;
	attr.config1 = conf->config1;
	attr.sample_period = conf->sample_period;
	attr.precise_ip = 1;
	attr.exclude_guest = 1;
	attr.sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_ADDR | PERF_SAMPLE_CPU |
		PERF_SAMPLE_WEIGHT | PERF_SAMPLE_CALLCHAIN;
	attr.disabled = 1;

	if ((fds[0] = pf_event_open(&attr, -1, cpu->cpuid, -1, 0)) < 0) {
		debug_print(NULL, 2, "pf_ll_setup: pf_event_open is failed "
			"for CPU%d\n", cpu->cpuid);
		fds[0] = INVALID_FD;
		return (-1);
	}
	
	if ((cpu->map_base = mmap(NULL, s_mapsize, PROT_READ | PROT_WRITE,
		MAP_SHARED, fds[0], 0)) == MAP_FAILED) {
		close(fds[0]);
		fds[0] = INVALID_FD;
		return (-1);
	}

	cpu->map_len = s_mapsize;
	cpu->map_mask = s_mapmask;
	return (0);
}

int
pf_ll_start(struct _perf_cpu *cpu)
{
	if (cpu->fds[0] != INVALID_FD) {
		return (ioctl(cpu->fds[0], PERF_EVENT_IOC_ENABLE, 0));
	}
	
	return (0);
}

int
pf_ll_stop(struct _perf_cpu *cpu)
{
	if (cpu->fds[0] != INVALID_FD) {
		return (ioctl(cpu->fds[0], PERF_EVENT_IOC_DISABLE, 0));
	}
	
	return (0);
}

static int
ll_sample_read(struct perf_event_mmap_page *mhdr, int size,
	pf_ll_rec_t *rec)
{
	struct { uint32_t pid, tid; } id;
	uint64_t i, addr, cpu, weight, nr, value, *ips;
	int j, ret = -1;

	/*
	 * struct read_format {
	 *	{ u32	pid, tid; }
	 *	{ u64	addr; }
	 *	{ u64	cpu; }
	 *	[ u64	nr; }
	 *	{ u64   ips[nr]; }
	 *	{ u64	weight; }
	 * };
	 */
	if (mmap_buffer_read(mhdr, &id, sizeof (id)) == -1) {
		debug_print(NULL, 2, "ll_sample_read: read pid/tid failed.\n");
		goto L_EXIT;
	}

	size -= sizeof (id);

	if (mmap_buffer_read(mhdr, &addr, sizeof (addr)) == -1) {
		debug_print(NULL, 2, "ll_sample_read: read addr failed.\n");
		goto L_EXIT;
	}

	size -= sizeof (addr);

	if (mmap_buffer_read(mhdr, &cpu, sizeof (cpu)) == -1) {
		debug_print(NULL, 2, "ll_sample_read: read cpu failed.\n");
		goto L_EXIT;
	}

	size -= sizeof (cpu);

	if (mmap_buffer_read(mhdr, &nr, sizeof (nr)) == -1) {
		debug_print(NULL, 2, "ll_sample_read: read nr failed.\n");
		goto L_EXIT;
	}

	size -= sizeof (nr);

	j = 0;
	ips = rec->ips;
	for (i = 0; i < nr; i++) {
		if (j >= IP_NUM) {
			break;
		}

		if (mmap_buffer_read(mhdr, &value, sizeof (value)) == -1) {
			debug_print(NULL, 2, "ll_sample_read: read ip failed.\n");
			goto L_EXIT;
		}

		size -= sizeof (value);
		
		if (is_userspace(value)) {
			/*
			 * Only save the user-space address.
			 */
			ips[j] = value;
			j++;
		}
	}

	if (mmap_buffer_read(mhdr, &weight, sizeof (weight)) == -1) {
		debug_print(NULL, 2, "ll_sample_read: read weight failed.\n");
		goto L_EXIT;
	}

	size -= sizeof (weight);
	
	rec->ip_num = j;
	rec->pid = id.pid;
	rec->tid = id.tid;
	rec->addr = addr;
	rec->cpu = cpu;
	rec->latency = weight;	
	ret = 0;

L_EXIT:
	if (size > 0) {
		mmap_buffer_skip(mhdr, size);
		debug_print(NULL, 2, "ll_sample_read: skip %d bytes, ret=%d\n",
			size, ret);
	}

	return (ret);
}

static void
ll_recbuf_update(pf_ll_rec_t *rec_arr, int *nrec, pf_ll_rec_t *rec)
{
	int i;

	if ((rec->pid == 0) || (rec->tid == 0)) {
		/* Just consider the user-land process/thread. */
		return;	
	}

	/*
	 * The size of array is enough.
	 */
	i = *nrec;
	memcpy(&rec_arr[i], rec, sizeof (pf_ll_rec_t));
	*nrec += 1;
}

void
pf_ll_record(struct _perf_cpu *cpu, pf_ll_rec_t *rec_arr, int *nrec)
{
	struct perf_event_mmap_page *mhdr = cpu->map_base;
	struct perf_event_header ehdr;
	pf_ll_rec_t rec;
	int size;

	*nrec = 0;

	for (;;) {
		if (mmap_buffer_read(mhdr, &ehdr, sizeof(ehdr)) == -1) {
			/* No valid record in ring buffer. */
			return;
 		}

		if ((size = ehdr.size - sizeof (ehdr)) <= 0) {
			return;
		}

		if ((ehdr.type == PERF_RECORD_SAMPLE) && (rec_arr != NULL)) {
			if (ll_sample_read(mhdr, size, &rec) == 0) {
				ll_recbuf_update(rec_arr, nrec, &rec);
			} else {
				/* No valid record in ring buffer. */
				return;	
			}
		} else {
			mmap_buffer_skip(mhdr, size);
		}
	}
}

void
pf_resource_free(struct _perf_cpu *cpu)
{
	int i;

	for (i = 0; i < PERF_COUNT_NUM; i++) {
		if (cpu->fds[i] != INVALID_FD) {
			close(cpu->fds[i]);
			cpu->fds[i] = INVALID_FD;
		}
	}

	if (cpu->map_base != MAP_FAILED) {
		munmap(cpu->map_base, cpu->map_len);
		cpu->map_base = MAP_FAILED;
		cpu->map_len = 0;
	}
}

int
pf_pqos_occupancy_setup(struct _perf_pqos *pqos, int pid, int lwpid)
{
	return 0;
}

int
pf_pqos_totalbw_setup(struct _perf_pqos *pqos, int pid, int lwpid)
{
	return 0;
}

int
pf_pqos_localbw_setup(struct _perf_pqos *pqos, int pid, int lwpid)
{
	return 0;
}

int
pf_pqos_start(struct _perf_pqos *pqos)
{
	return 0;
}

int
pf_pqos_stop(struct _perf_pqos *pqos)
{
	return 0;
}

static int
read_fd(int fd, void *buf, int bytes)
{
	int left = bytes, ret;

	while (left > 0) {
		if (((ret = read(fd, buf, left)) < 0) &&
			(errno == EINTR)) {
			debug_print(NULL, 2, "read_fd: read fd %d failed (ret = %d, errno = %d)\n",
				fd, ret, errno);
			continue;
		}

		if (ret < 0) {
			debug_print(NULL, 2, "read_fd: read fd %d failed (ret = %d)\n",
				fd, ret);
			return ret;
		}

		left -= ret;
		buf += ret;
	}

	return 0;
}

void
pf_pqos_record(struct _perf_pqos *pqos)
{

}

void
pf_pqos_resource_free(struct _perf_pqos *pqos)
{
	memset(pqos, 0, sizeof(struct _perf_pqos));
}

void
pf_uncoreqpi_free(struct _node *node)
{
	int i;
	node_qpi_t *qpi = &node->qpi;

	for (i = 0; i < qpi->qpi_num; i++) {
		if (qpi->qpi_info[i].fd != INVALID_FD) {
			debug_print(NULL, 2, "pf_uncoreqpi_free: nid %d, qpi %d, fd %d\n",
				node->nid, i, qpi->qpi_info[i].fd);
			close(qpi->qpi_info[i].fd);
		}

		qpi->qpi_info[i].fd = INVALID_FD;
		qpi->qpi_info[i].value_scaled = 0;
		memset(qpi->qpi_info[i].values, 0, sizeof(qpi->qpi_info[i].values));
	}
}

int
pf_uncoreqpi_setup(struct _node *node)
{
	struct perf_event_attr attr;
	node_qpi_t *qpi = &node->qpi;
	int i;

	for (i = 0; i < qpi->qpi_num; i++) {
		if (qpi->qpi_info[i].type == 0)
			continue;

		qpi->qpi_info[i].value_scaled = 0;
		memset(qpi->qpi_info[i].values, 0, sizeof(qpi->qpi_info[i].values));

		memset(&attr, 0, sizeof (attr));
		attr.type = qpi->qpi_info[i].type;
		attr.size = sizeof(attr);
		attr.config = qpi->qpi_info[i].config;
		attr.disabled = 1;
		attr.inherit = 1;
		attr.read_format =
			PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

		if ((qpi->qpi_info[i].fd = pf_event_open(&attr, -1,
			node->cpus[0].cpuid, -1, 0)) < 0) {
			debug_print(NULL, 2, "pf_uncoreqpi_setup: pf_event_open is failed "
				"for node %d, qpi %d, cpu %d, type %d, config 0x%x\n",
				node->nid, i, node->cpus[0].cpuid, attr.type, attr.config);
			qpi->qpi_info[i].fd = INVALID_FD;
			return (-1);
		}

		debug_print(NULL, 2, "pf_uncoreqpi_setup: pf_event_open is successful "
			"for node %d, qpi %d, cpu %d, type %d, config 0x%x, fd %d\n",
			node->nid, i, node->cpus[0].cpuid, attr.type, attr.config,
			qpi->qpi_info[i].fd);
	}

	return (0);
}

int pf_uncoreqpi_start(struct _node *node)
{
	node_qpi_t *qpi = &node->qpi;
	int i;

	for (i = 0; i < qpi->qpi_num; i++) {
		if (qpi->qpi_info[i].fd != INVALID_FD) {
			debug_print(NULL, 2, "pf_uncorqpi_start: "
				"for node %d, qpi %d, cpu %d, type %d, fd %d\n",
				node->nid, i, node->cpus[0].cpuid, qpi->qpi_info[i].type,
				qpi->qpi_info[i].fd);
			ioctl(qpi->qpi_info[i].fd, PERF_EVENT_IOC_ENABLE, 0);
		}
	}

	return (0);
}

int pf_uncoreqpi_smpl(struct _node *node)
{
	node_qpi_t *qpi = &node->qpi;
	uint64_t values[3];
	int i;

	for (i = 0; i < qpi->qpi_num; i++) {
		if (qpi->qpi_info[i].fd != INVALID_FD) {

			/*
			 * struct read_format {
			 *	{ u64	value; }
			 *	{ u64	time_enabled; }
			 *	{ u64	time_running; }
			 * };
			 */

			if (read_fd(qpi->qpi_info[i].fd, values,
				sizeof(values)) != 0) {

				debug_print(NULL, 2,
					"pf_uncoreqpi_smpl: read fd %d fail\n",
					qpi->qpi_info[i].fd);
				continue;
			}

			qpi->qpi_info[i].value_scaled = scale(
				values[0] - qpi->qpi_info[i].values[0],
				values[1] - qpi->qpi_info[i].values[1],
				values[2] - qpi->qpi_info[i].values[2]);

			debug_print(NULL, 2, "pf_uncoreqpi_smpl: "
				"node %d, qpi %d, fd %d: "
				"%" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n",
				node->nid, i, qpi->qpi_info[i].fd,
				values[0] - qpi->qpi_info[i].values[0],
				values[1] - qpi->qpi_info[i].values[1],
				values[2] - qpi->qpi_info[i].values[2]);

			memcpy(qpi->qpi_info[i].values, values, sizeof(values));
		}
	}

	return 0;
}

void
pf_uncoreimc_free(struct _node *node)
{
	int i;
	node_imc_t *imc = &node->imc;

	for (i = 0; i < imc->imc_num; i++) {
		if (imc->imc_info[i].fd != INVALID_FD) {
			debug_print(NULL, 2, "pf_uncoreimc_free: nid %d, imc %d, fd %d\n",
				node->nid, i, imc->imc_info[i].fd);
			close(imc->imc_info[i].fd);
		}

		imc->imc_info[i].fd = INVALID_FD;
		imc->imc_info[i].value_scaled = 0;
		memset(imc->imc_info[i].values, 0, sizeof(imc->imc_info[i].values));
	}
}

int
pf_uncoreimc_setup(struct _node *node)
{
	struct perf_event_attr attr;
	node_imc_t *imc = &node->imc;
	int i;

	for (i = 0; i < imc->imc_num; i++) {
		if (imc->imc_info[i].type == 0)
			continue;

		imc->imc_info[i].value_scaled = 0;
		memset(imc->imc_info[i].values, 0, sizeof(imc->imc_info[i].values));
		
		memset(&attr, 0, sizeof (attr));
		attr.type = imc->imc_info[i].type;
		attr.size = sizeof(attr);
		attr.config = 0xff04;
		attr.disabled = 1;
		attr.inherit = 1;
		attr.read_format =
			PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

		if ((imc->imc_info[i].fd = pf_event_open(&attr, -1,
			node->cpus[0].cpuid, -1, 0)) < 0) {
			debug_print(NULL, 2, "pf_uncoreimc_setup: pf_event_open is failed "
				"for node %d, imc %d, cpu %d, type %d, config 0x%x\n",
				node->nid, i, node->cpus[0].cpuid, attr.type, attr.config);
			imc->imc_info[i].fd = INVALID_FD;
			return (-1);
		}

		debug_print(NULL, 2, "pf_uncoreimc_setup: pf_event_open is successful "
			"for node %d, imc %d, cpu %d, type %d, config 0x%x, fd %d\n",
			node->nid, i, node->cpus[0].cpuid, attr.type, attr.config,
			imc->imc_info[i].fd);
	}

	return (0);
}

int pf_uncoreimc_start(struct _node *node)
{
	node_imc_t *imc = &node->imc;
	int i;
	
	for (i = 0; i < imc->imc_num; i++) {
		if (imc->imc_info[i].fd != INVALID_FD) {
			debug_print(NULL, 2, "pf_uncoreimc_start: "
				"for node %d, imc %d, cpu %d, type %d, fd %d\n",
				node->nid, i, node->cpus[0].cpuid, imc->imc_info[i].type,
				imc->imc_info[i].fd);
			ioctl(imc->imc_info[i].fd, PERF_EVENT_IOC_ENABLE, 0);
		}
	}

	return (0);
}

int pf_uncoreimc_smpl(struct _node *node)
{
	node_imc_t *imc = &node->imc;
	uint64_t values[3];
	int i;

	for (i = 0; i < imc->imc_num; i++) {
		if (imc->imc_info[i].fd != INVALID_FD) {

			/*
			 * struct read_format {
			 *	{ u64	value; }
			 *	{ u64	time_enabled; }
			 *	{ u64	time_running; }
			 * };
			 */

			if (read_fd(imc->imc_info[i].fd, values,
				sizeof(values)) != 0) {

				debug_print(NULL, 2,
					"pf_uncoreimc_smpl: read fd %d fail\n",
					imc->imc_info[i].fd);
				continue;
			}

			imc->imc_info[i].value_scaled = scale(
				values[0] - imc->imc_info[i].values[0],
				values[1] - imc->imc_info[i].values[1],
				values[2] - imc->imc_info[i].values[2]);

			debug_print(NULL, 2, "pf_uncoreimc_smpl: "
				"node %d, imc %d, fd %d: "
				"%" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n",
				node->nid, i, imc->imc_info[i].fd,
				values[0] - imc->imc_info[i].values[0],
				values[1] - imc->imc_info[i].values[1],
				values[2] - imc->imc_info[i].values[2]);

			memcpy(imc->imc_info[i].values, values, sizeof(values));
		}
	}
	
	return 0;
}
