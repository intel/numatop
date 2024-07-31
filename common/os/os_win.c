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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <signal.h>
#include <curses.h>
#include "../include/types.h"
#include "../include/util.h"
#include "../include/disp.h"
#include "../include/reg.h"
#include "../include/lwp.h"
#include "../include/proc.h"
#include "../include/page.h"
#include "../include/perf.h"
#include "../include/win.h"
#include "../include/ui_perf_map.h"
#include "../include/os/node.h"
#include "../include/os/os_perf.h"
#include "../include/os/os_util.h"
#include "../include/os/plat.h"
#include "../include/os/os_win.h"

/*
 * Build the readable string for caption line.
 * (window type: "WIN_TYPE_NODE_OVERVIEW")
 */
void
os_nodeoverview_caption_build(char *buf, int size)
{
	(void) snprintf(buf, size,
	    "%5s%12s%12s%11s%11s%11s%12s",
	    CAPTION_NID, CAPTION_MEM_ALL, CAPTION_MEM_FREE,
	    CAPTION_RMA, CAPTION_LMA, CAPTION_RL, CAPTION_CPU);
}

void
os_nodeoverview_data_build(char *buf, int size, nodeoverview_line_t *line,
    node_t *node)
{
	win_countvalue_t *value = &line->value;
	char mem_all[32], mem_free[32];

	(void) snprintf(mem_all, sizeof (mem_all), "%.1fG", line->mem_all);
	(void) snprintf(mem_free, sizeof (mem_free), "%.1fG", line->mem_free);

	if (plat_offcore_num() > 1) {
		(void) snprintf(buf, size,
		    "%5d%12s%12s%11.1f%11.1f%11.1f%11.1f",
		    node->nid, mem_all, mem_free, value->rma, value->lma,
		    value->rl, value->cpu * 100);
	} else {
		(void) snprintf(buf, size,
		    "%5d%12s%12s%11.1f%11s%11s%11.1f",
		    node->nid, mem_all, mem_free, value->rma,
		    "-", "-", value->cpu * 100);		
	}
}

static int
cpuid_cmp(const void *a, const void *b)
{
	const int *id1 = (const int *)a;
	const int *id2 = (const int *)b;

	if (*id1 > *id2) {
		return (1);
	}

	if (*id1 < *id2) {
		return (-1);
	}

	return (0);
}

/*
 * Build a readable string of CPU ID and try to reduce the string length. e.g.
 * For cpu1, cpu2, cpu3, cpu4, the string is "CPU(1-4)",
 * For cpu1, cpu3, cpu5, cpu7, the string is "CPU(1 3 5 7)"
 */
static void
node_cpu_string(node_t *node, char *s1, int size)
{
	char s2[128], s3[128];
	int i, j, k, l, cpuid_start;
	int *cpuid_arr;
	int ncpus;
	int s1_len = size;
	perf_cpu_t *cpus = node_cpus(node);

	s1[0] = 0;
	if ((ncpus = node->ncpus) == 0) {
		(void) strncpy(s1, "-", size);
		return;
	}

	if ((cpuid_arr = zalloc(sizeof (int) * ncpus)) == NULL) {
		return;
	}

	j = 0;
	for (i = 0; (i < NCPUS_NODE_MAX) && (j < ncpus); i++) {
		if ((cpus[i].cpuid != INVALID_CPUID) && (!cpus[i].hotremove)) {
			cpuid_arr[j++] = cpus[i].cpuid;
		}
	}

	qsort(cpuid_arr, ncpus, sizeof (int), cpuid_cmp);
	cpuid_start = cpuid_arr[0];

	if (ncpus == 1) {
		(void) snprintf(s1, size, "%d", cpuid_start);
        	free(cpuid_arr);
		return;
	}

	l = 1;
	k = 1;

	for (j = 1; j < ncpus; j++) {
		k++;
		if (cpuid_arr[j] != cpuid_start + l) {
			int s2_len = sizeof(s2);

			if (k < ncpus) {
				if (l == 1) {
					(void) snprintf(s2, sizeof (s2), "%d ", cpuid_start);
				} else {
					(void) snprintf(s2, sizeof (s2),
						"%d-%d ", cpuid_start, cpuid_start + l - 1);
				}
          		} else {
				if (l == 1) {
					(void) snprintf(s2, sizeof (s2), "%d",
						cpuid_start);
				} else {
					(void) snprintf(s2, sizeof (s2), "%d-%d",
						cpuid_start, cpuid_start + l - 1);
				}
				s2_len -= strlen(s2);

				(void) snprintf(s3, sizeof (s3), " %d",
					cpuid_arr[j]);
				s2_len -= strlen(s3);
				if (s2_len > 0)
					(void) strncat(s2, s3, s2_len);
			}

			s1_len -= strlen(s2);
			if (s1_len > 0)
				(void) strncat(s1, s2, s1_len);
          		cpuid_start = cpuid_arr[j];
           		l = 1;
		} else {
	        	if (k == ncpus) {
        	    		(void) snprintf(s2, sizeof (s2), "%d-%d",
                			cpuid_start, cpuid_start + l);
				s1_len -= strlen(s2);
				if (s1_len > 0)
					(void) strncat(s1, s2, s1_len);
       			} else {
            			l++;
       			}
       		}
	}

	free(cpuid_arr);
}

static void
nodedetail_line_show(win_reg_t *reg, char *title, char *value, int line)
{
	char s1[256];

	snprintf(s1, sizeof (s1), "%-30s%15s", title, value);
	reg_line_write(reg, line, ALIGN_LEFT, s1);
	dump_write("%s\n", s1);
}

/*
 * Display the performance statistics per node.
 */
void
os_nodedetail_data(dyn_nodedetail_t *dyn, win_reg_t *seg)
{
	char s1[256], s2[32];
	node_t *node;
	win_countvalue_t value;
	node_meminfo_t meminfo;
	int i = 1, j;
	node_qpi_t *qpi;
	node_imc_t *imc;
	uint64_t v = 0;

	reg_erase(seg);
	node = node_get(dyn->nid);
	win_node_countvalue(node, &value);
	node_meminfo(node->nid, &meminfo);

	/*
	 * Display the CPU
	 */
	node_cpu_string(node, s1, sizeof (s1));
	nodedetail_line_show(seg, "CPU:", s1, i++);

	/*
	 * Display the CPU utilization
	 */
	(void) snprintf(s1, sizeof (s1), "%.1f%%", value.cpu * 100.0);	
	nodedetail_line_show(seg, "CPU%:", s1, i++);

	/*
	 * Display the number of RMA
	 */
	(void) snprintf(s1, sizeof (s1), "%.1fK", value.rma);	
	nodedetail_line_show(seg, "RMA:", s1, i++);

	/*
	 * Display the number of LMA if platform supports
	 */
	if (plat_offcore_num() > 1) {
		(void) snprintf(s1, sizeof (s1), "%.1fK", value.lma);	
	} else {
		(void) snprintf(s1, sizeof (s1), "%s", "-");	
	}

	nodedetail_line_show(seg, "LMA:", s1, i++);

	/*
	 * Display the size of total memory
	 */
	(void) snprintf(s1, sizeof (s1), "%.1fG",
		(double)((double)(meminfo.mem_total) / (double)(GB_BYTES)));
	nodedetail_line_show(seg, "MEM total:", s1, i++);

	/*
	 * Display the size of free memory
	 */
	(void) snprintf(s1, sizeof (s1), "%.1fG",
		(double)((double)(meminfo.mem_free) / (double)(GB_BYTES)));
	nodedetail_line_show(seg, "MEM free:", s1, i++);

	/*
	 * Display the size of active memory.
	 */
	(void) snprintf(s1, sizeof (s1), "%.2fG",
		(double)((double)(meminfo.active) / (double)(GB_BYTES)));	
	nodedetail_line_show(seg, "MEM active:", s1, i++);

	/*
	 * Display the size of inactive memory.
	 */
	(void) snprintf(s1, sizeof (s1), "%.2fG",
		(double)((double)(meminfo.inactive) / (double)(GB_BYTES)));
	nodedetail_line_show(seg, "MEM inactive:", s1, i++);

	/*
	 * Display the size of dirty memory.
	 */
	(void) snprintf(s1, sizeof (s1), "%.2fG",
		(double)((double)(meminfo.dirty) / (double)(GB_BYTES)));
	nodedetail_line_show(seg, "Dirty:", s1, i++);

	/*
	 * Display the size of writeback memory.
	 */
	(void) snprintf(s1, sizeof (s1), "%.2fG",
		(double)((double)(meminfo.dirty) / (double)(GB_BYTES)));
	nodedetail_line_show(seg, "Writeback:", s1, i++);

	/*
	 * Display the size of mapped memory.
	 */
	(void) snprintf(s1, sizeof (s1), "%.2fG",
		(double)((double)(meminfo.mapped) / (double)(GB_BYTES)));
	nodedetail_line_show(seg, "Mapped:", s1, i++);

	/*
	 * Display the QPI link bandwidth
	 */
	qpi = &node->qpi;

	for (j = 0; j < qpi->qpi_num; j++) {		
		snprintf(s1, sizeof (s1), "%.1fMB", 
			ratio(qpi->qpi_info[j].value_scaled * 8, 1024 * 1024));
		snprintf(s2, sizeof (s2), "QPI/UPI %d bandwidth:", j);
		nodedetail_line_show(seg, s2, s1, i++);	
	}

	/*
	 * Display the memory controller bandwidth
	 */
	imc = &node->imc;

	for (j = 0; j < imc->imc_num; j++) {
		v += imc->imc_info[j].value_scaled * 64;
	}

	snprintf(s1, sizeof (s1), "%.1fMB", ratio(v, 1024 * 1024));
	nodedetail_line_show(seg,  "Memory controller bandwidth:", s1, i++);	

	reg_refresh_nout(seg);
}

static void
callchain_str_build(char *buf, int size, int idx, void *pv)
{
	callchain_line_t *lines = (callchain_line_t *)pv;
	callchain_line_t *line = &lines[idx];

	if (strlen(line->content) > 0) {
		strncpy(buf, line->content, size);
	} else {
		strncpy(buf, " ", size);
	}

	buf[size - 1] = 0;
}

static int
chainlist_show(sym_chainlist_t *chainlist, win_reg_t *reg)
{
	sym_callchain_t *chain;
	int nentry, nchain;
	int i, j = 0, k, nlines;
	char content[WIN_LINECHAR_MAX];
	callchain_line_t *buf, *line;

	sym_callchain_resort(chainlist);
	nentry = sym_chainlist_nentry(chainlist, &nchain);

	reg_erase(reg);
	if (nentry == 0) {
		snprintf(content, WIN_LINECHAR_MAX,
			"<- Detecting call-chain ... -> ");
		reg_line_write(reg, 0, ALIGN_LEFT, content);
		dump_write("%s\n", content);
		reg_refresh_nout(reg);
		return (0);
	}
	
	nlines = nentry + 2 * nchain;
	if ((buf = zalloc(nlines * sizeof (callchain_line_t))) == NULL) {
		return (-1);
	}
	
	for (i = 0; i < nchain; i++) {
		if ((chain = sym_callchain_detach(chainlist)) == NULL) {
			break;
		}
		
		line = &buf[j++];
		snprintf(line->content, WIN_LINECHAR_MAX,
			"<- call-chain %d: ->", i + 1);

		for (k = 0; k < chain->nentry; k++) {
			line = &buf[j++];			
			strncpy(line->content, chain->entry_arr[k].name, WIN_LINECHAR_MAX);
			line->content[WIN_LINECHAR_MAX - 1] = 0;
		}

		line = &buf[j++];
		strcpy(line->content, "");
		sym_callchain_free(chain);
	}

	if (reg->buf != NULL) {
		free(reg->buf);
	}

	reg->buf = (void *)buf;
	reg->nlines_total = nlines - 1;
	reg_scroll_show(reg, (void *)(reg->buf), nlines - 1, callchain_str_build);
	reg_refresh_nout(reg);
	sym_chainlist_free(chainlist);
	return (0);
}

int
os_callchain_list_show(dyn_callchain_t *dyn, track_proc_t *proc,
	track_lwp_t *lwp)
{
	perf_countchain_t *count_chain;
	perf_chainrecgrp_t *rec_grp;
	perf_chainrec_t *rec_arr;
	sym_chainlist_t chainlist;
	win_reg_t *reg;
	char content[WIN_LINECHAR_MAX];
	int i, j;
	int n_perf_count;
	perf_count_id_t *perf_count_ids = NULL;

	reg = &dyn->caption;
	reg_erase(reg);	
	snprintf(content, WIN_LINECHAR_MAX, "Call-chain list:");
	reg_line_write(reg, 1, ALIGN_LEFT, content);
	dump_write("%s\n", content);
	reg_refresh_nout(reg);

	reg = &dyn->pad;
	reg_erase(reg);	
	dump_write("\n");
	reg_refresh_nout(reg);

	if (lwp != NULL) {
		count_chain = &lwp->count_chain;
	} else {
		count_chain = &proc->count_chain;
	}

	if (sym_load(proc, SYM_TYPE_FUNC) != 0) {
		debug_print(NULL, 2, "Failed to load the process symbol "
			"(pid = %d)\n", proc->pid);
		return (-1);
	}

	memset(&chainlist, 0, sizeof (sym_chainlist_t));

	n_perf_count = get_ui_perf_count_map(dyn->ui_countid, &perf_count_ids);

	for (i = 0; i < n_perf_count; i++) {
		rec_grp = &count_chain->chaingrps[perf_count_ids[i]];
		rec_arr = rec_grp->rec_arr;

		for (j = 0; j < rec_grp->nrec_cur; j++) {
			sym_callchain_add(&proc->sym, rec_arr[j].callchain.ips,
				rec_arr[j].callchain.ip_num, &chainlist);
		}
	}

	chainlist_show(&chainlist, &dyn->data);
	return (0);
}

/*
 * The implementation of displaying window on screen for
 * window type "WIN_TYPE_LAT_PROC" and "WIN_TYPE_LAT_LWP"
 */
boolean_t
os_lat_win_draw(dyn_win_t *win)
{
	dyn_lat_t *dyn = (dyn_lat_t *)(win->dyn);
	track_proc_t *proc;
	boolean_t note_out, ret;

	if (!perf_ll_started()) {		
		win_warn_msg(WARN_LL_NOT_SUPPORT);
		win_note_show(NOTE_LAT);
		return (B_FALSE);
	}

	if ((proc = proc_find(dyn->pid)) == NULL) {
		win_warn_msg(WARN_INVALID_PID);
		win_note_show(NOTE_INVALID_PID);
		return (B_FALSE);
	}

	if (map_proc_load(proc) != 0) {
		proc_refcount_dec(proc);
		win_warn_msg(WARN_INVALID_MAP);
		win_note_show(NOTE_INVALID_MAP);
		return (B_FALSE);
	}

	win_title_show();
	ret = win_lat_data_show(proc, dyn, &note_out);
	if (!note_out) {
		win_note_show(NOTE_LAT);
	}

	proc_refcount_dec(proc);
	reg_update_all();
	return (ret);
}

void *
os_llcallchain_dyn_create(page_t *page)
{
	dyn_llcallchain_t *dyn;
	cmd_llcallchain_t *cmd = CMD_LLCALLCHAIN(&page->cmd);
	int i;

	if ((dyn = zalloc(sizeof (dyn_llcallchain_t))) == NULL) {
		return (NULL);
	}

	dyn->pid = cmd->pid;
	dyn->lwpid = cmd->lwpid;
	dyn->addr = cmd->addr;
	dyn->size = cmd->size;
	strncpy(dyn->desc, cmd->desc, WIN_DESCBUF_SIZE);
	dyn->desc[WIN_DESCBUF_SIZE - 1] = 0;

	if ((i = reg_init(&dyn->msg, 0, 1, g_scr_width, 2, A_BOLD)) < 0)
		goto L_EXIT;
	if ((i = reg_init(&dyn->buf_caption, 0, i, g_scr_width, 2, A_BOLD | A_UNDERLINE)) < 0)
		goto L_EXIT;
	if ((i = reg_init(&dyn->buf_data, 0, i, g_scr_width, 1, 0)) < 0)
		goto L_EXIT;
	if ((i = reg_init(&dyn->chain_caption, 0, i, g_scr_width, 2, A_BOLD | A_UNDERLINE)) < 0)
		goto L_EXIT;
	if ((i = reg_init(&dyn->pad, 0, i, g_scr_width, 1, 0)) < 0)
		goto L_EXIT;
	if ((reg_init(&dyn->chain_data, 0, i, g_scr_width, g_scr_height - i - 2, 0)) < 0)
		goto L_EXIT;
	reg_buf_init(&dyn->chain_data, NULL, win_callchain_line_get);
	reg_scroll_init(&dyn->chain_data, B_TRUE);

	return (dyn);
L_EXIT:
	free(dyn);
	return (NULL);
}

void
os_llcallchain_win_destroy(dyn_win_t *win)
{
	dyn_llcallchain_t *dyn;

	if ((dyn = win->dyn) != NULL) {
		if (dyn->chain_data.buf != NULL) {
			free(dyn->chain_data.buf);
		}

		reg_win_destroy(&dyn->msg);
		reg_win_destroy(&dyn->buf_caption);
		reg_win_destroy(&dyn->buf_data);
		reg_win_destroy(&dyn->chain_caption);
		reg_win_destroy(&dyn->pad);		
		reg_win_destroy(&dyn->chain_data);
		free(dyn);
	}
}

/*
 * The callback function used in bsearch() to compare the linear address.
 */
static int
bufaddr_cmp(const void *p1, const void *p2)
{
	const uint64_t addr = (const uint64_t)(uintptr_t)p1;
	const bufaddr_t *bufaddr = (const bufaddr_t *)p2;

	if (addr < bufaddr->addr) {
		return (-1);
	}

	if (addr >= bufaddr->addr + bufaddr->size) {
		return (1);
	}

	return (0);
}

static int
llcallchain_bufinfo_show(dyn_llcallchain_t *dyn, track_proc_t *proc,
	track_lwp_t *lwp)
{
	char content[WIN_LINECHAR_MAX];
	lat_line_t *lat_buf, *line;
	win_reg_t *reg;
	int lwpid = 0, nlines, lat;
	
	/*
	 * Display the caption of data table:
	 * "ADDR SIZE ACCESS% LAT(ns) DESC"
	 */
	reg = &dyn->buf_caption;
	reg_erase(reg);	
	(void) snprintf(content, sizeof (content),
	    "%16s%8s%11s%11s%34s",
	    CAPTION_ADDR, CAPTION_SIZE, CAPTION_BUFHIT,
	    CAPTION_AVGLAT, CAPTION_DESC);
	reg_line_write(&dyn->buf_caption, 1, ALIGN_LEFT, content);
	dump_write("%s\n", content);
	reg_refresh_nout(reg);
	
	/*
	 * Get the stat of buffer.
	 */	
	reg = &dyn->buf_data;
	reg_erase(reg);	

	if (lwp != NULL) {
		lwpid = lwp->id;	
	}
	 
	if ((lat_buf = win_lat_buf_create(proc, lwpid, &nlines)) == NULL) {
		return (-1);
	}

	/*
	 * Fill in the memory access information.
	 */
	win_lat_buf_fill(lat_buf, nlines, proc, lwp, &lat);
	
	/*
	 * Check if the linear address is located in a buffer in
	 * process address space.
	 */ 
	if ((line = bsearch((void *)(uintptr_t)(dyn->addr), lat_buf, nlines,
		sizeof (lat_line_t), bufaddr_cmp)) != NULL) {
		win_lat_str_build(content, WIN_LINECHAR_MAX, 0, line);
		reg_line_write(reg, 0, ALIGN_LEFT, content);
		dump_write("%s\n", content);
	}
	
	reg_refresh_nout(reg);
	free(lat_buf);
	return (0);
}

static int
llcallchain_list_show(dyn_llcallchain_t *dyn, track_proc_t *proc,
	track_lwp_t *lwp)
{
	perf_llrecgrp_t *llrec_grp;
	char content[WIN_LINECHAR_MAX];
	os_perf_llrec_t *rec_arr;
	sym_chainlist_t chainlist;
	win_reg_t *reg;
	int i;

	reg = &dyn->chain_caption;
	reg_erase(reg);	
	snprintf(content, WIN_LINECHAR_MAX, "Call-chain list:");
	reg_line_write(reg, 1, ALIGN_LEFT, content);
	dump_write("%s\n", content);
	reg_refresh_nout(reg);

	reg = &dyn->pad;
	reg_erase(reg);	
	dump_write("\n");
	reg_refresh_nout(reg);

	if (lwp != NULL) {
		llrec_grp = &lwp->llrec_grp;
	} else {
		llrec_grp = &proc->llrec_grp;
	}

	if (sym_load(proc, SYM_TYPE_FUNC) != 0) {
		debug_print(NULL, 2, "Failed to load the process symbol "
			"(pid = %d)\n", proc->pid);
		return (-1);
	}

	memset(&chainlist, 0, sizeof (sym_chainlist_t));
	rec_arr = llrec_grp->rec_arr;

	for (i = 0; i < llrec_grp->nrec_cur; i++) {
		if ((rec_arr[i].addr < dyn->addr) ||
			(rec_arr[i].addr >= dyn->addr + dyn->size)) {
			continue;
		}

		sym_callchain_add(&proc->sym, rec_arr[i].callchain.ips,
			rec_arr[i].callchain.ip_num, &chainlist);		
	}

	chainlist_show(&chainlist, &dyn->chain_data);
	return (0);
}

static void
llcallchain_data_show(dyn_win_t *win, boolean_t *note_out)
{
	dyn_llcallchain_t *dyn;
	pid_t pid;
	int lwpid;
	uint64_t size;
	track_proc_t *proc;
	track_lwp_t *lwp = NULL;
	win_reg_t *reg;
	char content[WIN_LINECHAR_MAX], intval_buf[16];
	char size_str[32];

	dyn = (dyn_llcallchain_t *)(win->dyn);
	pid = dyn->pid;
	lwpid = dyn->lwpid;
	size = dyn->size;
	*note_out = B_FALSE;

	if ((proc = proc_find(pid)) == NULL) {
		win_warn_msg(WARN_INVALID_PID);
		win_note_show(NOTE_INVALID_PID);
		*note_out = B_TRUE;
		return;
	}

	if ((lwpid > 0) && ((lwp = proc_lwp_find(proc, lwpid)) == NULL)) {
		proc_refcount_dec(proc);
		win_warn_msg(WARN_INVALID_LWPID);
		win_note_show(NOTE_INVALID_LWPID);
		*note_out = B_TRUE;
		return;
	}

	reg = &dyn->msg;
	reg_erase(reg);	
	disp_intval(intval_buf, 16);
	win_size2str(size, size_str, sizeof (size_str));

	if (lwp == NULL) {
		(void) snprintf(content, WIN_LINECHAR_MAX,
			"Call-chain when process accesses the memory area (pid: %d)"
	    	" (interval: %s)", pid, intval_buf);
	} else {
		(void) snprintf(content, WIN_LINECHAR_MAX,
			"Call-chain when thread accesses the memory area (lwpid: %d)"
	    	" (interval: %s)", lwpid, intval_buf);
	}

	dump_write("\n*** %s\n", content);
	reg_line_write(reg, 1, ALIGN_LEFT, content);
	reg_refresh_nout(reg);

	llcallchain_bufinfo_show(dyn, proc, lwp);
	llcallchain_list_show(dyn, proc, lwp);
	
	if (lwp != NULL) {
		lwp_refcount_dec(lwp);
	}

	proc_refcount_dec(proc);
}

/*
 * Display window on screen.
 * (window type: "WIN_TYPE_LLCALLCHAIN")
 */
boolean_t
os_llcallchain_win_draw(dyn_win_t *win)
{
	boolean_t note_out;

	win_title_show();
	llcallchain_data_show(win, &note_out);
	if (!note_out) {
		win_note_show(NOTE_LLCALLCHAIN);
	}

	reg_update_all();
	return (B_TRUE);
}

void
os_llcallchain_win_scroll(dyn_win_t *win, int scroll_type)
{
	dyn_llcallchain_t *dyn = (dyn_llcallchain_t *)(win->dyn);

	reg_line_scroll(&dyn->chain_data, scroll_type);
}

/*
 * "lat_buf" points to an array which contains the process address mapping.
 * Each item in array represents a buffer in process address space. "rec"
 * points to a SMPL record. The function is responsible for checking and
 * comparing the record with the process address mapping.
 */
void
os_lat_buf_hit(lat_line_t *lat_buf, int nlines, os_perf_llrec_t *rec,
	uint64_t *total_lat, uint64_t *total_sample)
{
	lat_line_t *line;

	/*
	 * Check if the linear address is located in a buffer in
	 * process address space.
	 */
	if ((line = bsearch((void *)(uintptr_t)(rec->addr), lat_buf, nlines,
	    sizeof (lat_line_t), bufaddr_cmp)) != NULL) {
		/*
		 * If the linear address is located in, that means this
		 * buffer is accessed, so update the statistics of accessing.
		 */
		line->naccess++;
		line->latency += rec->latency;
		*total_lat += rec->latency;
		*total_sample += 1;
	}
}


static lat_line_t *
latnode_buf_create(track_proc_t *proc, int lwpid, uint64_t addr,
	uint64_t size, int *nlines)
{
	map_entry_t *entry;
	numa_entry_t *numa_entry;
	numa_map_t *numa_map;
	lat_line_t *buf;
	int i;

	if ((entry = map_entry_find(proc, addr, size)) == NULL) {
		return (NULL);
	}
	
	numa_map = &entry->numa_map;
	*nlines = numa_map->nentry_cur;
	if ((buf = zalloc(sizeof (lat_line_t) * (*nlines))) == NULL) {
		return (NULL);
	}

	for (i = 0; i < *nlines; i++) {
		numa_entry = &numa_map->arr[i];
		buf[i].pid = proc->pid;
		buf[i].lwpid = lwpid;
		buf[i].nid_show = B_TRUE;
		buf[i].bufaddr.addr = numa_entry->start_addr;
		buf[i].bufaddr.size = numa_entry->end_addr - numa_entry->start_addr;
		buf[i].nid = numa_entry->nid;		
	}

	return (buf);
}

static int
latnode_data_get(track_proc_t *proc, track_lwp_t *lwp, dyn_latnode_t *dyn)
{
	lat_line_t *buf;
	char content[WIN_LINECHAR_MAX];
	int nlines, lwpid = 0, lat = 0;

	reg_erase(&dyn->caption);
	reg_refresh_nout(&dyn->caption);
	reg_erase(&dyn->data);
	reg_refresh_nout(&dyn->data);

	if (lwp != NULL) {
		lwpid = lwp->id;		
	}

	if ((buf = latnode_buf_create(proc, lwpid, dyn->addr,
		dyn->size, &nlines)) == NULL) {
		reg_line_write(&dyn->caption, 1, ALIGN_LEFT,
		    "Failed to get the process NUMA mapping!");
		reg_refresh_nout(&dyn->caption);
		return (-1);
	}

	win_lat_buf_fill(buf, nlines, proc, lwp, &lat);

	/*
	 * Sort by the number of buffer accessing.
	 */
	qsort(buf, nlines, sizeof (lat_line_t), win_lat_cmp);

	/*
	 * Display the caption of data table:
	 * "ADDR SIZE NODE ACCESS% LAT(ns) DESC"
	 */
	(void) snprintf(content, sizeof (content),
	    "%16s%8s%8s%11s%11s",
	    CAPTION_ADDR, CAPTION_SIZE, CAPTION_NID, CAPTION_BUFHIT,
	    CAPTION_AVGLAT);

	reg_line_write(&dyn->caption, 1, ALIGN_LEFT, content);
	reg_refresh_nout(&dyn->caption);

	/*
	 * Save data of buffer statistics in scrolling buffer.
	 */
	dyn->data.nlines_total = nlines;
	if (dyn->data.buf != NULL) {
		free(dyn->data.buf);
	}

	/*
	 * Display the buffer with statistics in scrolling buffer
	 */
	dyn->data.buf = (void *)buf;
	reg_scroll_show(&dyn->data, (void *)(dyn->data.buf),
	    nlines, win_lat_str_build);
	reg_refresh_nout(&dyn->data);

	return (0);
}

static boolean_t
latnode_data_show(track_proc_t *proc, dyn_latnode_t *dyn,
	map_entry_t *entry __attribute__((unused)),
	boolean_t *note_out)
{
	win_reg_t *reg;
	track_lwp_t *lwp = NULL;
	char content[WIN_LINECHAR_MAX], intval_buf[16], size_str[32];

	*note_out = B_FALSE;

	if ((dyn->lwpid != 0) &&
	    (lwp = proc_lwp_find(proc, dyn->lwpid)) == NULL) {
		win_warn_msg(WARN_INVALID_LWPID);
		win_note_show(NOTE_INVALID_LWPID);
		*note_out = B_TRUE;
		return (B_FALSE);
	}

	reg = &dyn->msg;
	reg_erase(reg);
	disp_intval(intval_buf, 16);
	(void) snprintf(content, sizeof (content),
	    "Break down of memory area for physical memory on node (interval: %s)",
	    intval_buf);
	reg_line_write(reg, 1, ALIGN_LEFT, content);
	dump_write("\n*** %s\n", content);
	reg_refresh_nout(reg);

	reg = &dyn->note;
	reg_erase(reg);	
	win_size2str(dyn->size, size_str, sizeof (size_str));
	if (lwp != NULL) {
		(void) snprintf(content, sizeof (content),
		    "Memory area(%"PRIX64", %s), thread(%d)",
		    dyn->addr, size_str, lwp->id);
	} else {
		(void) snprintf(content, sizeof (content),
		    "Memory area(%"PRIX64", %s), process(%d)",
		    dyn->addr, size_str, proc->pid);
	}

	reg_line_write(reg, 1, ALIGN_LEFT, content);
	dump_write("\n*** %s\n", content);
	reg_refresh_nout(reg);

	latnode_data_get(proc, lwp, dyn);

	if (lwp != NULL) {
		lwp_refcount_dec(lwp);
	}

	return (B_TRUE);
}

/*
 * The implementation of displaying window on screen for
 * window type "WIN_TYPE_LATNODE_PROC" and "WIN_TYPE_LATNODE_LWP"
 */
boolean_t
os_latnode_win_draw(dyn_win_t *win)
{
	dyn_latnode_t *dyn = (dyn_latnode_t *)(win->dyn);
	track_proc_t *proc;
	map_entry_t *entry;
	boolean_t note_out, ret;

	if ((proc = proc_find(dyn->pid)) == NULL) {
		win_warn_msg(WARN_INVALID_PID);
		win_note_show(NOTE_INVALID_PID);
		return (B_FALSE);
	}

	if ((entry = map_entry_find(proc, dyn->addr, dyn->size)) == NULL) {
		proc_refcount_dec(proc);
		win_warn_msg(WARN_INVALID_MAP);
		win_note_show(NOTE_INVALID_MAP);
		return (B_FALSE);
	}

	if (map_map2numa(proc, entry) != 0) {
		proc_refcount_dec(proc);
		win_warn_msg(WARN_INVALID_NUMAMAP);
		win_note_show(NOTE_INVALID_NUMAMAP);
		return (B_FALSE);		
	}

	win_title_show();
	ret = latnode_data_show(proc, dyn, entry, &note_out);
	if (!note_out) {
		win_note_show(NOTE_LATNODE);
	}

	proc_refcount_dec(proc);
	reg_update_all();
	return (ret);
}
