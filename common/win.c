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

/* This file contains code to create/show/destroy a window on screen. */

#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <curses.h>
#include <sys/mman.h>
#include "include/types.h"
#include "include/util.h"
#include "include/node.h"
#include "include/plat.h"
#include "include/disp.h"
#include "include/reg.h"
#include "include/lwp.h"
#include "include/proc.h"
#include "include/page.h"
#include "include/perf.h"

static boolean_t s_first_load = B_TRUE;
static win_reg_t s_note_reg;
static win_reg_t s_title_reg;

/*
 * Build the readable string for caption line.
 * (window type: "WIN_TYPE_TOPNPROC")
 */
static void
topnproc_caption_build(char *buf, int size)
{
	char tmp[32];

	switch (g_sortkey) {
	case SORT_KEY_CPU:
		(void) snprintf(tmp, sizeof (tmp), "*%s", CAPTION_CPU);
		(void) snprintf(buf, size,
		    "%6s%15s%11s%11s%11s%11s%11s",
		    CAPTION_PID, CAPTION_PROC, CAPTION_RPI,
		    CAPTION_LPI, CAPTION_RL, CAPTION_CPI, tmp);
		break;

	case SORT_KEY_CPI:
		(void) snprintf(tmp, sizeof (tmp), "*%s", CAPTION_CPI);
		(void) snprintf(buf, size,
		    "%6s%15s%11s%11s%11s%11s%11s",
		    CAPTION_PID, CAPTION_PROC, CAPTION_RPI,
		    CAPTION_LPI, CAPTION_RL, tmp, CAPTION_CPU);
		break;

	case SORT_KEY_RPI:
		(void) snprintf(tmp, sizeof (tmp), "*%s", CAPTION_RPI);
		(void) snprintf(buf, size,
		     "%6s%15s%11s%11s%11s%11s%11s",
		    CAPTION_PID, CAPTION_PROC, tmp,
		    CAPTION_LPI, CAPTION_RL, CAPTION_CPI, CAPTION_CPU);
		break;

	case SORT_KEY_LPI:
		(void) snprintf(tmp, sizeof (tmp), "*%s", CAPTION_LPI);
		(void) snprintf(buf, size,
		    "%6s%15s%11s%11s%11s%11s%11s",
		    CAPTION_PID, CAPTION_PROC, CAPTION_RPI,
		    tmp, CAPTION_RL, CAPTION_CPI, CAPTION_CPU);
		break;

	case SORT_KEY_RL:
		(void) snprintf(tmp, sizeof (tmp), "*%s", CAPTION_RL);
		(void) snprintf(buf, size,
		    "%6s%15s%11s%11s%11s%11s%11s",
		    CAPTION_PID, CAPTION_PROC, CAPTION_RPI,
		    CAPTION_LPI, tmp, CAPTION_CPI, CAPTION_CPU);
		break;

	default:
		(void) snprintf(buf, size,
		    "%6s%15s%11s%11s%11s%11s%11s",
		    CAPTION_PID, CAPTION_PROC, CAPTION_RPI,
		    CAPTION_LPI, CAPTION_RL, CAPTION_CPI, CAPTION_CPU);
		break;
	}
}

static void
topnproc_data_build(char *buf, int size, topnproc_line_t *line)
{
	win_countvalue_t *value = &line->value;

	if (plat_offcore_num() > 1) {
		(void) snprintf(buf, size,
	    	"%6d%15s%11.1f%11.1f%11.1f%11.2f%10.1f",
	    	line->pid, line->proc_name, value->rpi, value->lpi,
	    	value->rl, value->cpi, value->cpu * 100);
	} else {
		(void) snprintf(buf, size,
	    	"%6d%15s%11.1f%11s%11s%11.2f%10.1f",
	    	line->pid, line->proc_name, value->rpi, "-",
	    	"-", value->cpi, value->cpu * 100);		
	}
}

/*
 * Build the readable string for data line.
 * (window type: "WIN_TYPE_TOPNPROC")
 */
static void
topnproc_str_build(char *buf, int size, int idx, void *pv)
{
	topnproc_line_t *lines = (topnproc_line_t *)pv;
	topnproc_line_t *line = &lines[idx];

	topnproc_data_build(buf, size, line);
}

/*
 * Build the readable string for scrolling line.
 * (window type: "WIN_TYPE_TOPNPROC")
 */
static void
topnproc_line_get(win_reg_t *reg, int idx, char *line, int size)
{
	topnproc_line_t *lines;

	lines = (topnproc_line_t *)(reg->buf);
	topnproc_str_build(line, size, idx, (void *)lines);
}

/*
 * Build the readable string for caption line.
 * (window type: "WIN_TYPE_RAW_NUM")
 */
static void
rawnum_caption_build(char *buf, int size)
{
	char tmp[32];

	switch (g_sortkey) {
	case SORT_KEY_CPU:
		(void) snprintf(tmp, sizeof (tmp), "*%s", CAPTION_CPU);
		(void) snprintf(buf, size,
		    "%6s%15s%11s%11s%11s%11s%11s",
		    CAPTION_PID, CAPTION_PROC, CAPTION_RMA,
		    CAPTION_LMA, CAPTION_RL, CAPTION_CPI, tmp);
		break;

	case SORT_KEY_CPI:
		(void) snprintf(tmp, sizeof (tmp), "*%s", CAPTION_CPI);
		(void) snprintf(buf, size,
		    "%6s%15s%11s%11s%11s%11s%11s",
		    CAPTION_PID, CAPTION_PROC, CAPTION_RMA,
		    CAPTION_LMA, CAPTION_RL, tmp, CAPTION_CPU);
		break;

	case SORT_KEY_RMA:
		(void) snprintf(tmp, sizeof (tmp), "*%s", CAPTION_RMA);
		(void) snprintf(buf, size,
		    "%6s%15s%11s%11s%11s%11s%11s",
		    CAPTION_PID, CAPTION_PROC, tmp,
		    CAPTION_LMA, CAPTION_RL, CAPTION_CPI, CAPTION_CPU);
		break;

	case SORT_KEY_LMA:
		(void) snprintf(tmp, sizeof (tmp), "*%s", CAPTION_LMA);
		(void) snprintf(buf, size,
		    "%6s%15s%11s%11s%11s%11s%11s",
		    CAPTION_PID, CAPTION_PROC, CAPTION_RMA,
		    tmp, CAPTION_RL, CAPTION_CPI, CAPTION_CPU);
		break;

	case SORT_KEY_RL:
		(void) snprintf(tmp, sizeof (tmp), "*%s", CAPTION_RL);
		(void) snprintf(buf, size,
		    "%6s%15s%11s%11s%11s%11s%11s",
		    CAPTION_PID, CAPTION_PROC, CAPTION_RMA,
		    CAPTION_LMA, tmp, CAPTION_CPI, CAPTION_CPU);
		break;

	default:
		(void) snprintf(buf, size,
		    "%6s%15s%11s%11s%11s%11s%11s",
		    CAPTION_PID, CAPTION_PROC, CAPTION_RMA,
		    CAPTION_LMA, CAPTION_RL, CAPTION_CPI, CAPTION_CPU);
		break;
	}
}

static void
rawnum_data_build(char *buf, int size, topnproc_line_t *line)
{
	win_countvalue_t *value = &line->value;

	if (plat_offcore_num() > 1) {
		(void) snprintf(buf, size,
		    "%6d%15s%11.1f%11.1f%11.1f%11.2f%10.1f",
		    line->pid, line->proc_name, value->rma, value->lma,
		    value->rl, value->cpi, value->cpu * 100);
	} else {
		(void) snprintf(buf, size,
		    "%6d%15s%11.1f%11s%11s%11.2f%10.1f",
		    line->pid, line->proc_name, value->rma, "-",
		    "-", value->cpi, value->cpu * 100);		
	}
}

/*
 * Build the readable string of data line
 * (window type: "WIN_TYPE_RAW_NUM") 
 */
static void
rawnum_str_build(char *buf, int size, int idx, void *pv)
{
	topnproc_line_t *lines = (topnproc_line_t *)pv;
	topnproc_line_t *line = &lines[idx];

	rawnum_data_build(buf, size, line);
}

/*
 * Build the readable string for scrolling line.
 * (window type: "WIN_TYPE_RAW_NUM")
 */
static void
rawnum_line_get(win_reg_t *reg, int idx, char *line, int size)
{
	topnproc_line_t *lines;

	lines = (topnproc_line_t *)(reg->buf);
	rawnum_str_build(line, size, idx, (void *)lines);
}

/*
 * Initialize the display layout.
 * (window type: "WIN_TYPE_TOPNPROC"/"WIN_TYPE_RAW_NUM")
 */
static dyn_topnproc_t *
topnproc_dyn_create(int type)
{
	dyn_topnproc_t *dyn;
	void *buf;
	int i;

	if ((dyn = zalloc(sizeof (dyn_topnproc_t))) == NULL) {
		return (NULL);
	}

	if ((buf = zalloc(sizeof (topnproc_line_t) *
	    WIN_NLINES_MAX)) == NULL) {
		free(dyn);
		return (NULL);
	}

	i = reg_init(&dyn->summary, 0, 1, g_scr_width, 2, A_BOLD);
	i = reg_init(&dyn->caption, 0, i, g_scr_width, 2, A_BOLD | A_UNDERLINE);
	i = reg_init(&dyn->data, 0, i, g_scr_width, g_scr_height - i - 5, 0);

	if (type == WIN_TYPE_TOPNPROC) {
		reg_buf_init(&dyn->data, buf, topnproc_line_get);
	} else {
		reg_buf_init(&dyn->data, buf, rawnum_line_get);
	}

	reg_scroll_init(&dyn->data, B_TRUE);
	reg_init(&dyn->hint, 0, i, g_scr_width, g_scr_height - i - 1, A_BOLD);
	return (dyn);
}

/*
 * Release the resources of window.
 * (window type: "WIN_TYPE_TOPNPROC"/"WIN_TYPE_RAW_NUM")
 */
static void
topnproc_win_destroy(dyn_win_t *win)
{
	dyn_topnproc_t *dyn;

	if ((dyn = win->dyn) != NULL) {
		if (dyn->data.buf != NULL) {
			free(dyn->data.buf);
			dyn->data.buf = NULL;
		}

		reg_win_destroy(&dyn->summary);
		reg_win_destroy(&dyn->caption);
		reg_win_destroy(&dyn->data);
		reg_win_destroy(&dyn->hint);
		free(dyn);		
	}
}

/*
 * Seperate the value of metrics by raw perf data.
 */
static int
win_countvalue_fill(win_countvalue_t *cv,
	count_value_t *countval_arr, int cpuid_max, int nid, int ms, int ncpus)
{
	uint64_t rma, lma, ir, clk, all_clks;
	double d;

	rma = node_countval_sum(countval_arr, cpuid_max, nid, COUNT_RMA);
	lma = node_countval_sum(countval_arr, cpuid_max, nid, COUNT_LMA);
	clk = node_countval_sum(countval_arr, cpuid_max, nid, COUNT_CLK);
	ir = node_countval_sum(countval_arr, cpuid_max, nid, COUNT_IR);		

	cv->rpi = ratio(rma * 1000, ir);
	cv->lpi = ratio(lma * 1000, ir);
	cv->cpi = ratio(clk, ir);
	cv->rma = ratio(rma, 1000);
	cv->lma = ratio(lma, 1000);
	cv->rl = ratio(rma, lma);

	d = (double)ms / MS_SEC;
	all_clks = (uint64_t)(d * (double)g_clkofsec * (double)ncpus);
	cv->cpu = ratio(clk, all_clks);	
	return (0);
}

/*
 * Convert the perf data to the required format and copy
 * the converted result out via "line".
 * (window type: "WIN_TYPE_TOPNPROC")
 */
static void
topnproc_data_save(track_proc_t *proc, int intval, topnproc_line_t *line)
{
	(void) memset(line, 0, sizeof (topnproc_line_t));

	/*
	 * Cut off the process name if it's too long.
	 */
	strncpy(line->proc_name, proc->name, sizeof (line->proc_name));
	line->proc_name[sizeof (line->proc_name) - 1] = 0;

	line->proc_name[WIN_PROCNAME_SIZE - 1] = 0;
	line->pid = proc->pid;
	line->nlwp = proc_nlwp(proc);

	win_countvalue_fill(&line->value, proc->countval_arr, proc->cpuid_max,
		NODE_ALL, intval, g_ncpus);
}

static void
topnproc_data_show(dyn_win_t *win)
{
	dyn_topnproc_t *dyn;
	win_reg_t *reg, *data_reg;
	char content[WIN_LINECHAR_MAX], intval_buf[16];
	int nprocs, nlwps, i;
	track_proc_t *proc;
	int intval;
	topnproc_line_t *lines;

	dyn = (dyn_topnproc_t *)(win->dyn);
	data_reg = &dyn->data;

	/* Get the number of total processes and total threads */
	proc_lwp_count(&nprocs, &nlwps);
	nprocs = MIN(nprocs, WIN_NLINES_MAX);
	data_reg->nlines_total = nprocs;
	
	/*
	 * Convert the sampling interval (nanosecond) to a human readable string.
	 */
	disp_intval(intval_buf, 16);

	/*
	 * Display the summary message:
	 * "Monitoring xxx processes and yyy threads (interval: zzzs)"
	 */
	(void) snprintf(content, sizeof (content),
	    "Monitoring %d processes and %d threads (interval: %s)",
	    nprocs, nlwps, intval_buf);

	reg = &dyn->summary;
	reg_erase(reg);
	reg_line_write(reg, 1, ALIGN_LEFT, content);
	dump_write("\n*** %s\n", content);
	reg_refresh_nout(reg);

	/*
	 * Display the caption of table:
	 * "PID PROC/NLWP RMA(K) LMA(K) CPI CPU%"
	 */
	reg = &dyn->caption;
	if (win->type == WIN_TYPE_TOPNPROC) {
		topnproc_caption_build(content, sizeof (content));
	} else {
		rawnum_caption_build(content, sizeof (content));
	}

	reg_erase(reg);
	reg_line_write(reg, 1, ALIGN_LEFT, content);
	dump_write("%s\n", content);
	reg_refresh_nout(reg);

	reg_erase(data_reg);
	lines = (topnproc_line_t *)(data_reg->buf);

	/*
	 * Sort the processes by specified metric which is indicated by g_sortkey
	 */
	proc_group_lock();
	proc_resort(g_sortkey);

	/*
	 * Save the perf data of processes in scrolling buffer.
	 */
	for (i = 0; i < nprocs; i++) {
		if ((proc = proc_sort_next()) == NULL) {
			break;
		}

		intval = proc_intval_get(proc);
		topnproc_data_save(proc, intval, &lines[i]);
	}

	/*
	 * Display the processes with metrics in scrolling buffer 
	 */
	if (win->type == WIN_TYPE_TOPNPROC) {
		reg_scroll_show(data_reg, (void *)lines, nprocs,
		    topnproc_str_build);
	} else {
		reg_scroll_show(data_reg, (void *)lines, nprocs,
		    rawnum_str_build);
	}

	proc_group_unlock();
	reg_refresh_nout(data_reg);

	/*
	 * Dispaly hint message for window type
	 * "WIN_TYPE_TOPNPROC" and "WIN_TYPE_RAW_NUM"
	 */
	reg = &dyn->hint;
	reg_erase(reg);

	if (win->type == WIN_TYPE_TOPNPROC) {
		reg_line_write(reg, 1, ALIGN_LEFT,
		    "<- Hotkey for sorting: 1(RPI), 2(LPI), 3(RMA/LMA), 4(CPI), 5(CPU%%) ->");
	} else {
		reg_line_write(reg, 1, ALIGN_LEFT,
		    "<- Hotkey for sorting: 1(RMA), 2(LMA), 3(RMA/LMA), 4(CPI), 5(CPU%%) ->");
	}

	reg_line_write(reg, 2, ALIGN_LEFT,
	    "CPU%% = system CPU utilization");
	
	reg_refresh_nout(reg);
}

/*
 * Show the message "Loading ..." on screen
 */
static void
load_msg_show(void)
{
	char content[64];
	win_reg_t reg;

	(void) snprintf(content, sizeof (content), "Loading ...");

	reg_init(&reg, 0, 1, g_scr_width, g_scr_height - 1, A_BOLD);
	reg_erase(&reg);
	reg_line_write(&reg, 1, ALIGN_LEFT, content);
	reg_refresh(&reg);
	reg_win_destroy(&reg);
}

/*
 * Show the title "NumaTop v1.0, (C) 2012 Intel Corporation"
 */
static void
title_show(void)
{
	reg_erase(&s_title_reg);
	reg_line_write(&s_title_reg, 0, ALIGN_MIDDLE, NUMATOP_TITLE);
	reg_refresh_nout(&s_title_reg);
}

/*
 * Show the note information at the bottom of window"
 */
static void
note_show(char *note)
{
	char *content;

	content = (note != NULL) ? note : NOTE_DEFAULT;
	reg_erase(&s_note_reg);
	reg_line_write(&s_note_reg, 0, ALIGN_LEFT, content);
	reg_refresh(&s_note_reg);
	reg_update_all();
}

/*
 * Display window on screen.
 * (window type: "WIN_TYPE_TOPNPROC" and "WIN_TYPE_RAW_NUM")
 */
static boolean_t
topnproc_win_draw(dyn_win_t *win)
{	
	title_show();
	if (s_first_load) {
		s_first_load = B_FALSE;
		load_msg_show();
		note_show(NULL);
		reg_update_all();
		return (B_TRUE);
	}

	topnproc_data_show(win);

	if (win->type == WIN_TYPE_TOPNPROC) {
		note_show(NOTE_TOPNPROC);
	} else {
		note_show(NOTE_TOPNPROC_RAW);
	}

	reg_update_all();
	return (B_TRUE);
}

/*
 * The function would be called when user hits the <UP>/<DOWN> key
 * to scroll data line.
 * (window type: "WIN_TYPE_TOPNPROC" and "WIN_TYPE_RAW_NUM") 
 */
static void
topnproc_win_scroll(dyn_win_t *win, int scroll_type)
{
	dyn_topnproc_t *dyn = (dyn_topnproc_t *)(win->dyn);

	reg_line_scroll(&dyn->data, scroll_type);
}

/*
 * The function would be called when user hits the "ENTER" key
 * on selected data line.
 * (window type: "WIN_TYPE_TOPNPROC" and "WIN_TYPE_RAW_NUM") 
 */
static void
topnproc_win_scrollenter(dyn_win_t *win)
{
	dyn_topnproc_t *dyn = (dyn_topnproc_t *)(win->dyn);
	win_reg_t *reg = &dyn->data;
	scroll_line_t *scroll = &reg->scroll;
	topnproc_line_t *lines;
	cmd_monitor_t cmd_monitor;
	boolean_t badcmd;

	if (scroll->highlight == -1) {
		return;
	}

	/*
	 * Construct a command to switch to next window
	 * (WIN_TYPE_MONIPROC).
	 */
	lines = (topnproc_line_t *)(reg->buf);
	cmd_monitor.id = CMD_MONITOR_ID;
	cmd_monitor.pid = lines[scroll->highlight].pid;
	cmd_monitor.lwpid = 0;

	/* LINTED E_BAD_PTR_CAST_ALIGN */
	cmd_execute((cmd_t *)(&cmd_monitor), &badcmd);
}

/*
 * Build the readable string for caption line.
 * (window type: "WIN_TYPE_MONIPROC"/"WIN_TYPE_MONILWP")
 */
static void
moni_caption_build(char *buf, int size)
{
	(void) snprintf(buf, size,
	    "%5s%10s%10s%11s%11s%10s%10s%10s",
	    CAPTION_NID, CAPTION_RPI, CAPTION_LPI,
	    CAPTION_RMA, CAPTION_LMA, CAPTION_RL,
	    CAPTION_CPI, CAPTION_CPU);
}

static void
moni_data_build(char *buf, int size, moni_line_t *line,
	node_t *node)
{
	win_countvalue_t *value = &line->value;

	if (plat_offcore_num() > 1) {
		(void) snprintf(buf, size,
		    "%5d%10.1f%10.1f%11.1f%11.1f%10.1f%10.2f%9.1f",
		    node->nid, value->rpi, value->lpi, value->rma, value->lma,
		    value->rl, value->cpi, value->cpu * 100);
	} else {
		(void) snprintf(buf, size,
		    "%5d%10.1f%10s%11.1f%11s%10s%10.2f%9.1f",
		    node->nid, value->rpi, "-", value->rma, "-", 
		    "-", value->cpi, value->cpu * 100);
	}
}

/*
 * Build the readable string for data line.
 * (window type: "WIN_TYPE_MONIPROC"/"WIN_TYPE_MONILWP")
 */
static void
moni_str_build(char *buf, int size, int idx, void *pv)
{
	moni_line_t *lines = (moni_line_t *)pv;
	moni_line_t *line = &lines[idx];
	node_t *node;
	
	if ((node = node_valid_get(idx)) != NULL) {
		moni_data_build(buf, size, line, node);
	}
}

/*
 * Build the readable string for scrolling line.
 * (window type: "WIN_TYPE_MONIPROC"/"WIN_TYPE_MONILWP)
 */
static void
moni_line_get(win_reg_t *reg, int idx, char *line, int size)
{
	moni_line_t *lines;

	lines = (moni_line_t *)(reg->buf);
	moni_str_build(line, size, idx, (void *)lines);
}

/*
 * Initialize the display layout.
 * (window type: "WIN_TYPE_MONIPROC")
 */
static dyn_moniproc_t *
moniproc_dyn_create(pid_t pid)
{
	dyn_moniproc_t *dyn;
	void *buf_cur;
	int i, nnodes;

	nnodes = node_num();
	if ((dyn = zalloc(sizeof (dyn_moniproc_t))) == NULL) {
		return (NULL);
	}

	if ((buf_cur = zalloc(sizeof (moni_line_t) * nnodes)) == NULL) {
		free(dyn);
		return (NULL);
	}

	dyn->pid = pid;

	i = reg_init(&dyn->msg, 0, 1, g_scr_width, 2, A_BOLD);
	i = reg_init(&dyn->caption_cur, 0, i, g_scr_width, 2, A_BOLD | A_UNDERLINE);
	i = reg_init(&dyn->data_cur, 0, i, g_scr_width, nnodes, 0);

	reg_buf_init(&dyn->data_cur, buf_cur, moni_line_get);
	reg_scroll_init(&dyn->data_cur, B_TRUE);

	reg_init(&dyn->hint, 0, i, g_scr_width, g_scr_height - i - 1, A_BOLD);
	return (dyn);
}

/*
 * Initialize the display layout.
 * (window type: "WIN_TYPE_MONILWP")
 */
static dyn_monilwp_t *
monilwp_dyn_create(pid_t pid, id_t lwpid)
{
	dyn_monilwp_t *dyn;
	void *buf_cur;
	int i, nnodes;

	nnodes = node_num();
	if ((dyn = zalloc(sizeof (dyn_monilwp_t))) == NULL) {
		return (NULL);
	}

	if ((buf_cur = zalloc(sizeof (moni_line_t) * nnodes)) == NULL) {
		free(dyn);
		return (NULL);
	}

	dyn->pid = pid;
	dyn->lwpid = lwpid;

	i = reg_init(&dyn->msg, 0, 1, g_scr_width, 2, A_BOLD);
	i = reg_init(&dyn->caption_cur, 0, i, g_scr_width, 2, A_BOLD | A_UNDERLINE);
	i = reg_init(&dyn->data_cur, 0, i, g_scr_width, nnodes, 0);

	reg_buf_init(&dyn->data_cur, buf_cur, moni_line_get);
	reg_scroll_init(&dyn->data_cur, B_TRUE);

	reg_init(&dyn->hint, 0, i, g_scr_width, g_scr_height - i - 1, A_BOLD);
	return (dyn);
}

/*
 * Convert the perf data to the required format and copy
 * the converted result out via "line".
 * (window type: "WIN_TYPE_MONIPROC")
 */
/* ARGSUSED */
static void
moniproc_data_save(track_proc_t *proc, int nid_idx, int nnodes,
	moni_line_t *line)
{
	int ncpus, intval;
	node_t *node;

	(void) memset(line, 0, sizeof (moni_line_t));
	if ((node = node_valid_get(nid_idx)) == NULL) {
		return;
	}

	line->nid = node->nid;
	line->pid = proc->pid;
	ncpus = node_ncpus(node);
	intval = proc_intval_get(proc);

	win_countvalue_fill(&line->value, proc->countval_arr,
		proc->cpuid_max, node->nid, intval, ncpus);
}

static boolean_t
moniproc_data_show(dyn_win_t *win, boolean_t *note_out)
{
	dyn_moniproc_t *dyn;
	win_reg_t *reg;
	char content[WIN_LINECHAR_MAX], intval_buf[16];
	pid_t pid;
	track_proc_t *proc;
	int i, nnodes;
	moni_line_t *lines;

	dyn = (dyn_moniproc_t *)(win->dyn);
	pid = dyn->pid;

	*note_out = B_FALSE;
	if ((proc = proc_find(pid)) == NULL) {
		win_warn_msg(WARN_INVALID_PID);
		note_show(NOTE_INVALID_PID);
		*note_out = B_TRUE;
		return (B_FALSE);
	}

	reg = &dyn->msg;
	disp_intval(intval_buf, 16);
	(void) snprintf(content, sizeof (content),
	    "Monitoring the process \"%s\" (%d) (interval: %s)",
	    proc->name, proc->pid, intval_buf);

	reg_erase(reg);
	reg_line_write(reg, 1, ALIGN_LEFT, content);
	dump_write("\n*** %s\n", content);
	reg_refresh_nout(reg);

	/*
	 * Display the caption of table:
	 * "NODE RPI(K) LPI(K) RMA(K) LMA(K) RMA/LMA CPI CPU%
	 */
	moni_caption_build(content, sizeof (content));
	reg = &dyn->caption_cur;
	reg_erase(reg);
	reg_line_write(reg, 1, ALIGN_LEFT, content);
	dump_write("%s\n", content);
	reg_refresh_nout(reg);

	nnodes = node_num();
	reg = &dyn->data_cur;
	reg_erase(reg);
	lines = (moni_line_t *)(reg->buf);
	reg->nlines_total = nnodes;

	/*
	 * Save the per-node data with metrics of a specified process
	 * in scrolling buffer.
	 */
	for (i = 0; i < nnodes; i++) {
		moniproc_data_save(proc, i, nnodes, &lines[i]);
	}

	/*
	 * Display the per-node data with metrics of a specified process
	 * in scrolling buffer 
	 */
	reg_scroll_show(reg, (void *)lines, nnodes, moni_str_build);
	reg_refresh_nout(reg);
	proc_refcount_dec(proc);

	/*
	 * Dispaly hint message for window type "WIN_TYPE_MONIPROC"
	 */
	reg = &dyn->hint;
	reg_erase(reg);
	reg_line_write(reg, reg->nlines_scr - 2, ALIGN_LEFT,
	    "CPU%% = per-node CPU utilization");
	reg_refresh_nout(reg);

	return (B_TRUE);
}

/*
 * Display window on screen.
 * (window type: "WIN_TYPE_MONIPROC")
 */
static boolean_t
moniproc_win_draw(dyn_win_t *win)
{
	boolean_t note_out, ret;

	title_show();
	ret = moniproc_data_show(win, &note_out);

	if (!note_out) {
		note_show(NOTE_MONIPROC);
	}

	reg_update_all();
	return (ret);
}

/*
 * Convert the perf data to the required format and copy
 * the converted result out via "line".
 * (window type: "WIN_TYPE_MONILWP")
 */
/* ARGSUSED */
static void
monilwp_data_save(track_lwp_t *lwp, int nid_idx, int nnodes,
	moni_line_t *line)
{
	node_t *node;
	int intval;

	(void) memset(line, 0, sizeof (moni_line_t));
	if ((node = node_valid_get(nid_idx)) == NULL) {
		return;
	}

	line->nid = node->nid;
	intval = lwp_intval_get(lwp);

	win_countvalue_fill(&line->value, lwp->countval_arr,
		lwp->cpuid_max, node->nid, intval, 1);
}

static boolean_t
monilwp_data_show(dyn_win_t *win, boolean_t *note_out)
{
	dyn_monilwp_t *dyn;
	win_reg_t *reg;
	char content[WIN_LINECHAR_MAX], intval_buf[16];
	pid_t pid;
	id_t lwpid;
	track_proc_t *proc;
	track_lwp_t *lwp;
	int i, nnodes;
	moni_line_t *lines;

	dyn = (dyn_monilwp_t *)(win->dyn);
	pid = dyn->pid;
	lwpid = dyn->lwpid;

	*note_out = B_FALSE;
	if ((proc = proc_find(pid)) == NULL) {
		win_warn_msg(WARN_INVALID_PID);
		note_show(NOTE_INVALID_PID);
		*note_out = B_TRUE;
		return (B_FALSE);
	}

	if ((lwp = proc_lwp_find(proc, lwpid)) == NULL) {
		proc_refcount_dec(proc);
		win_warn_msg(WARN_INVALID_LWPID);
		note_show(NOTE_INVALID_LWPID);
		*note_out = B_TRUE;
		return (B_FALSE);
	}

	reg = &dyn->msg;
	disp_intval(intval_buf, 16);
	(void) snprintf(content, sizeof (content),
	    "Monitoring the thread %d in \"%s\" (interval: %s)",
		lwpid, proc->name, intval_buf);

	reg_erase(reg);
	reg_line_write(reg, 1, ALIGN_LEFT, content);
	dump_write("\n*** %s\n", content);
	reg_refresh_nout(reg);

	/*
	 * Display the caption of table:
	 * "NODE RMA(K) LMA(K) RMA/LMA CPI CPU%
	 */
	moni_caption_build(content, sizeof (content));
	reg = &dyn->caption_cur;
	reg_erase(reg);
	reg_line_write(reg, 1, ALIGN_LEFT, content);
	dump_write("%s\n", content);
	reg_refresh_nout(reg);

	nnodes = node_num();
	reg = &dyn->data_cur;
	lines = (moni_line_t *)(reg->buf);
	reg->nlines_total = nnodes;
	reg_erase(reg);

	/*
	 * Save the per-node data with metrics of a specified thread
	 * in scrolling buffer.
	 */
	 for (i = 0; i < nnodes; i++) {
		monilwp_data_save(lwp, i, nnodes, &lines[i]);
	}

	/*
	 * Display the per-node data with metrics of a specified thread
	 * in scrolling buffer 
	 */
	reg_scroll_show(reg, (void *)lines, nnodes, moni_str_build);
	reg_refresh_nout(reg);
	lwp_refcount_dec(lwp);
	proc_refcount_dec(proc);

	/*
	 * Dispaly hint message for window type "WIN_TYPE_MONILWP"
	 */
	reg = &dyn->hint;
	reg_erase(reg);
	reg_line_write(reg, reg->nlines_scr - 2, ALIGN_LEFT,
	    "CPU%% = per-CPU CPU utilization");
	reg_refresh_nout(reg);

	return (B_TRUE);
}

/*
 * Display window on screen.
 * (window type: "WIN_TYPE_MONILWP")
 */
static boolean_t
monilwp_win_draw(dyn_win_t *win)
{
	boolean_t note_out, ret;

	title_show();
	ret = monilwp_data_show(win, &note_out);

	if (!note_out) {
		note_show(NOTE_MONILWP);
	}

	reg_update_all();
	return (ret);
}

/*
 * The common interface of initializing the screen layout for
 * window type "WIN_TYPE_MONIPROC" and "WIN_TYPE_MONILWP"
 */
static void *
moni_dyn_create(page_t *page, boolean_t (**draw)(dyn_win_t *), win_type_t *type)
{
	void *dyn;

	if (CMD_MONITOR(&page->cmd)->lwpid == 0) {
		if ((dyn = moniproc_dyn_create(
		    CMD_MONITOR(&page->cmd)->pid)) != NULL) {
			*draw = moniproc_win_draw;
			*type = WIN_TYPE_MONIPROC;
			return (dyn);
		}
	} else if ((dyn = monilwp_dyn_create(
	    CMD_MONITOR(&page->cmd)->pid,
	    CMD_MONITOR(&page->cmd)->lwpid)) != NULL) {
		*draw = monilwp_win_draw;
		*type = WIN_TYPE_MONILWP;
		return (dyn);
	}

	return (NULL);
}

/*
 * Release the resources of window.
 * (window type: "WIN_TYPE_MONIPROC")
 */
static void
moniproc_win_destroy(dyn_win_t *win)
{
	dyn_moniproc_t *dyn;

	if ((dyn = win->dyn) != NULL) {
		if (dyn->data_cur.buf != NULL) {
			free(dyn->data_cur.buf);
		}

		reg_win_destroy(&dyn->msg);
		reg_win_destroy(&dyn->caption_cur);
		reg_win_destroy(&dyn->data_cur);
		reg_win_destroy(&dyn->hint);
		free(dyn);		
	}
}

/*
 * The function would be called when user hits the <UP>/<DOWN> key
 * to scroll data line.
 * (window type: "WIN_TYPE_MONIPROC") 
 */
static void
moniproc_win_scroll(dyn_win_t *win, int scroll_type)
{
	dyn_moniproc_t *dyn = (dyn_moniproc_t *)(win->dyn);

	reg_line_scroll(&dyn->data_cur, scroll_type);
}

/*
 * The function would be called when user hits the "ENTER" key
 * on selected data line.
 * (window type: "WIN_TYPE_MONIPROC") 
 */
static void
moniproc_win_scrollenter(dyn_win_t *win)
{
	dyn_moniproc_t *dyn = (dyn_moniproc_t *)(win->dyn);
	win_reg_t *reg = &dyn->data_cur;
	scroll_line_t *scroll = &reg->scroll;
	moni_line_t *lines;
	cmd_lwp_t cmd_lwp;
	boolean_t badcmd;

	if (scroll->highlight == -1) {
		return;
	}

	/*
	 * Construct a command to switch to next window
	 * "WIN_TYPE_TOPNLWP".
	 */
	lines = (moni_line_t *)(reg->buf);
	cmd_lwp.id = CMD_LWP_ID;
	cmd_lwp.pid = lines[scroll->highlight].pid;

	/* LINTED E_BAD_PTR_CAST_ALIGN */
	cmd_execute((cmd_t *)(&cmd_lwp), &badcmd);
}

/*
 * Release the resources for window type "WIN_TYPE_MONILWP"
 */
static void
monilwp_win_destroy(dyn_win_t *win)
{
	dyn_monilwp_t *dyn;

	if ((dyn = win->dyn) != NULL) {
		if (dyn->data_cur.buf != NULL) {
			free(dyn->data_cur.buf);
		}

		reg_win_destroy(&dyn->msg);
		reg_win_destroy(&dyn->caption_cur);
		reg_win_destroy(&dyn->data_cur);
		reg_win_destroy(&dyn->hint);
		free(dyn);
	}
}

/*
 * The function would be called when user hits the <UP>/<DOWN> key
 * to scroll data line.
 * (window type: "WIN_TYPE_MONILWP") 
 */
static void
monilwp_win_scroll(dyn_win_t *win, int scroll_type)
{
	dyn_monilwp_t *dyn = (dyn_monilwp_t *)(win->dyn);

	reg_line_scroll(&dyn->data_cur, scroll_type);
}

/*
 * Build the readable string for caption line.
 * (window type: "WIN_TYPE_TOPNLWP")
 */
static void
topnlwp_caption_build(char *buf, int size)
{
	(void) snprintf(buf, size,
	    "%6s%10s%10s%11s%11s%10s%10s%10s",
	    CAPTION_LWP, CAPTION_RPI, CAPTION_LPI,
	    CAPTION_RMA, CAPTION_LMA, CAPTION_RL,
	    CAPTION_CPI, CAPTION_CPU);
}

static void
topnlwp_data_build(char *buf, int size, topnlwp_line_t *line)
{
	char tmp[32];
	win_countvalue_t *value = &line->value;

	(void) snprintf(tmp, sizeof (tmp), "%d", line->lwpid);

	if (plat_offcore_num() > 1) {
		(void) snprintf(buf, size,
		    "%6s%10.1f%10.1f%11.1f%11.1f%10.1f%10.2f%9.1f",
		    tmp, value->rpi, value->lpi, value->rma,
		    value->lma, value->rl, value->cpi, value->cpu * 100);
	} else {
		(void) snprintf(buf, size,
		    "%6s%10.1f%10s%11.1f%11s%10s%10.2f%9.1f",
		    tmp, value->rpi, "-", value->rma,
		    "-", "-", value->cpi, value->cpu * 100);		
	}
}

/*
 * Build the readable string for data line.
 * (window type: "WIN_TYPE_TOPNLWP")
 */
static void
topnlwp_str_build(char *buf, int size, int idx, void *pv)
{
	topnlwp_line_t *lines = (topnlwp_line_t *)pv;
	topnlwp_line_t *line = &lines[idx];

	topnlwp_data_build(buf, size, line);
}

/*
 * Build the readable string for scrolling line.
 * (window type: "WIN_TYPE_TOPNLWP")
 */
static void
topnlwp_line_get(win_reg_t *reg, int idx, char *line, int size)
{
	topnlwp_line_t *lines;

	lines = (topnlwp_line_t *)(reg->buf);
	topnlwp_str_build(line, size, idx, (void *)lines);
}

/*
 * Initialize the display layout.
 * (window type: "WIN_TYPE_TOPNLWP")
 */
static dyn_topnlwp_t *
topnlwp_dyn_create(page_t *page)
{
	dyn_topnlwp_t *dyn;
	void *buf;
	int i;
	cmd_lwp_t *cmd_lwp = (cmd_lwp_t *)(&page->cmd);
	pid_t pid = cmd_lwp->pid;

	if ((dyn = zalloc(sizeof (dyn_topnlwp_t))) == NULL) {
		return (NULL);
	}

	if ((buf = zalloc(sizeof (topnlwp_line_t) * WIN_NLINES_MAX)) == NULL) {
		free(dyn);
		return (NULL);
	}

	dyn->pid = pid;

	i = reg_init(&dyn->msg, 0, 1, g_scr_width, 2, A_BOLD);
	i = reg_init(&dyn->caption, 0, i, g_scr_width, 2, A_BOLD | A_UNDERLINE);
	i = reg_init(&dyn->data, 0, i, g_scr_width, g_scr_height - i - 4, 0);

	reg_buf_init(&dyn->data, buf, topnlwp_line_get);
	reg_scroll_init(&dyn->data, B_TRUE);

	reg_init(&dyn->hint, 0, i, g_scr_width, g_scr_height - i - 1, A_BOLD);
	return (dyn);
}

/*
 * Release the resources of window.
 * (window type: "WIN_TYPE_TOPNLWP")
 */
static void
topnlwp_win_destroy(dyn_win_t *win)
{
	dyn_topnlwp_t *dyn;

	if ((dyn = win->dyn) != NULL) {
		if (dyn->data.buf != NULL) {
			free(dyn->data.buf);
		}

		reg_win_destroy(&dyn->msg);
		reg_win_destroy(&dyn->caption);
		reg_win_destroy(&dyn->data);
		reg_win_destroy(&dyn->hint);
		free(dyn);
	}
}

/*
 * Convert the perf data to the required format and copy
 * the converted result out via "line".
 * (window type: "WIN_TYPE_TOPNLWP")
 */
static void
topnlwp_data_save(track_lwp_t *lwp, int intval, topnlwp_line_t *line)
{
	(void) memset(line, 0, sizeof (topnlwp_line_t));
	line->pid = lwp->proc->pid;
	line->lwpid = lwp->id;

	win_countvalue_fill(&line->value, lwp->countval_arr,
		lwp->cpuid_max, NODE_ALL, intval, 1);
}

static boolean_t
topnlwp_data_show(dyn_win_t *win, boolean_t *note_out)
{
	dyn_topnlwp_t *dyn;
	win_reg_t *reg;
	char content[WIN_LINECHAR_MAX], intval_buf[16];
	pid_t pid;
	track_proc_t *proc;
	int i, nlwps;
	topnlwp_line_t *lines;
	int intval;
	track_lwp_t *lwp;

	*note_out = B_FALSE;
	dyn = (dyn_topnlwp_t *)(win->dyn);
	pid = dyn->pid;
	if ((pid == -1) || ((proc = proc_find(pid)) == NULL)) {
		win_warn_msg(WARN_INVALID_PID);
		note_show(NOTE_INVALID_PID);
		*note_out = B_TRUE;
		return (B_FALSE);
	}

	/* Get the number of threads in the same process */
	nlwps = MIN(proc_nlwp(proc), WIN_NLINES_MAX);
	disp_intval(intval_buf, 16);

	(void) snprintf(content, sizeof (content),
	    "Monitoring all threads in \"%s\" (%d) (interval: %s)",
	    proc->name, proc->pid, intval_buf);

	reg = &dyn->msg;
	reg_erase(reg);
	reg_line_write(reg, 1, ALIGN_LEFT, content);
	dump_write("\n*** %s\n", content);
	reg_refresh_nout(reg);

	/*
	 * Display the caption of data table:
	 * "PID RPI LPI RMA(K) LMA(K) CPI CPU%"
	 */
	topnlwp_caption_build(content, sizeof (content));
	reg = &dyn->caption;
	reg_erase(reg);
	reg_line_write(reg, 1, ALIGN_LEFT, content);
	dump_write("%s\n", content);
	reg_refresh_nout(reg);

	reg = &dyn->data;
	reg_erase(reg);
	reg->nlines_total = nlwps;
	lines = (topnlwp_line_t *)(reg->buf);
	(void) pthread_mutex_lock(&proc->mutex);

	/*
	 * Sort the threads by the value of CPU utilization
	 */
	proc_lwp_resort(proc, SORT_KEY_CPU);

	/*
	 * Save the data of threads with metrics in scrolling buffer.
	 */
	for (i = 0; i < nlwps; i++) {
		if ((lwp = lwp_sort_next(proc)) == NULL) {
			break;
		}

		intval = lwp_intval_get(lwp);
		topnlwp_data_save(lwp, intval, &lines[i]);
	}

	/*
	 * Display the threads with metrics in scrolling buffer 
	 */
	reg_scroll_show(reg, (void *)lines, nlwps, topnlwp_str_build);
	(void) pthread_mutex_unlock(&proc->mutex);
	reg_refresh_nout(reg);
	proc_refcount_dec(proc);

	/*
	 * Display hint message for window type "WIN_TYPE_TOPNLWP"
	 */
	reg = &dyn->hint;
	reg_erase(reg);
	reg_line_write(reg, reg->nlines_scr - 2, ALIGN_LEFT,
	    "CPU%% = per-CPU CPU utilization");
	reg_refresh_nout(reg);

	return (B_TRUE);
}

/*
 * Display window on screen.
 * (window type: "WIN_TYPE_TOPNLWP")
 */
static boolean_t
topnlwp_win_draw(dyn_win_t *win)
{
	boolean_t note_out, ret;

	title_show();
	ret = topnlwp_data_show(win, &note_out);
	if (!note_out) {
		note_show(NOTE_TOPNLWP);
	}

	reg_update_all();
	return (ret);
}

/*
 * The function would be called when user hits the <UP>/<DOWN> key
 * to scroll data line.
 * (window type: "WIN_TYPE_TOPNLWP") 
 */
static void
topnlwp_win_scroll(dyn_win_t *win, int scroll_type)
{
	dyn_topnlwp_t *dyn = (dyn_topnlwp_t *)(win->dyn);

	reg_line_scroll(&dyn->data, scroll_type);
}

/*
 * The function would be called when user hits the "ENTER" key
 * on selected data line.
 * (window type: "WIN_TYPE_TOPNLWP") 
 */
static void
topnlwp_win_scrollenter(dyn_win_t *win)
{
	dyn_topnlwp_t *dyn = (dyn_topnlwp_t *)(win->dyn);
	win_reg_t *reg = &dyn->data;
	scroll_line_t *scroll = &reg->scroll;
	topnlwp_line_t *lines;
	cmd_monitor_t cmd_monitor;
	boolean_t badcmd;

	if (scroll->highlight == -1) {
		return;
	}

	/*
	 * Construct a command to switch to next window
	 * (WIN_TYPE_MONILWP)
	 */
	lines = (topnlwp_line_t *)(reg->buf);
	cmd_monitor.id = CMD_MONITOR_ID;
	cmd_monitor.pid = lines[scroll->highlight].pid;
	cmd_monitor.lwpid = lines[scroll->highlight].lwpid;

	/* LINTED E_BAD_PTR_CAST_ALIGN */
	cmd_execute((cmd_t *)(&cmd_monitor), &badcmd);
}

/*
 * Build the readable string for caption line.
 * (window type: "WIN_TYPE_NODE_OVERVIEW")
 */
static void
nodeoverview_caption_build(char *buf, int size)
{
	(void) snprintf(buf, size,
	    "%5s%12s%12s%11s%11s%11s%12s",
	    CAPTION_NID, CAPTION_MEM_ALL, CAPTION_MEM_FREE,
	    CAPTION_RMA, CAPTION_LMA, CAPTION_RL, CAPTION_CPU);
}

static void
nodeoverview_data_build(char *buf, int size, nodeoverview_line_t *line,
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

/*
 * Build the readable string for data line.
 * (window type: "WIN_TYPE_NODE_OVERVIEW")
 */
static void
nodeoverview_str_build(char *buf, int size, int idx, void *pv)
{
	nodeoverview_line_t *lines = (nodeoverview_line_t *)pv;
	nodeoverview_line_t *line = &lines[idx];
	node_t *node;
	
	if ((node = node_valid_get(idx)) != NULL) {
		nodeoverview_data_build(buf, size, line, node);
	}
}

/*
 * Build the readable string for scrolling line.
 * (window type: "WIN_TYPE_NODE_OVERVIEW")
 */
static void
nodeoverview_line_get(win_reg_t *reg, int idx, char *line, int size)
{
	nodeoverview_line_t *lines;

	lines = (nodeoverview_line_t *)(reg->buf);
	nodeoverview_str_build(line, size, idx, (void *)lines);
}

/*
 * Initialize the display layout for window type
 * "WIN_TYPE_NODE_OVERVIEW"
 */
static dyn_nodeoverview_t *
nodeoverview_dyn_create(void)
{
	dyn_nodeoverview_t *dyn;
	void *buf_cur;
	int i, nnodes;

	nnodes = node_num();
	if ((dyn = zalloc(sizeof (dyn_nodeoverview_t))) == NULL) {
		return (NULL);
	}

	if ((buf_cur = zalloc(sizeof (nodeoverview_line_t) * nnodes)) == NULL) {
		free(dyn);
		return (NULL);
	}

	i = reg_init(&dyn->msg, 0, 1, g_scr_width, 2, A_BOLD);
	i = reg_init(&dyn->caption_cur, 0, i, g_scr_width, 2, A_BOLD | A_UNDERLINE);
	i = reg_init(&dyn->data_cur, 0, i, g_scr_width, nnodes, 0);

	reg_buf_init(&dyn->data_cur, buf_cur, nodeoverview_line_get);
	reg_scroll_init(&dyn->data_cur, B_TRUE);

	reg_init(&dyn->hint, 0, i, g_scr_width, g_scr_height - i - 1, A_BOLD);
	return (dyn);
}

/*
 * Release the resources of window.
 * (window type: "WIN_TYPE_NODE_OVERVIEW")
 */
static void
nodeoverview_win_destroy(dyn_win_t *win)
{
	dyn_nodeoverview_t *dyn;

	if ((dyn = win->dyn) != NULL) {
		if (dyn->data_cur.buf != NULL) {
			free(dyn->data_cur.buf);
		}

		reg_win_destroy(&dyn->msg);
		reg_win_destroy(&dyn->caption_cur);
		reg_win_destroy(&dyn->data_cur);
		reg_win_destroy(&dyn->hint);
		free(dyn);
	}
}

static void
node_win_countvalue(node_t *node, win_countvalue_t *cv)
{
	double d;
	uint64_t rma, lma, clk, ir, all_clks;

	rma = node_countval_get(node, COUNT_RMA);
	lma = node_countval_get(node, COUNT_LMA);
	clk = node_countval_get(node, COUNT_CLK);
	ir = node_countval_get(node, COUNT_IR);

	cv->rpi = ratio(rma * 1000, ir);
	cv->lpi = ratio(lma * 1000, ir);
	cv->cpi = ratio(clk, ir);
	cv->rma = ratio(rma, 1000);
	cv->lma = ratio(lma, 1000);
	cv->rl = ratio(rma, lma);

	d = (double)node_intval_get() / MS_SEC;
	all_clks = (uint64_t)(d * (double)g_clkofsec * (double)node_ncpus(node));
	cv->cpu = ratio(clk, all_clks);
}

/*
 * Convert the perf data to the required format and copy
 * the converted result out via "line".
 * (window type: "WIN_TYPE_MONILWP")
 */
/* ARGSUSED */
static void
nodeoverview_data_save(int nid_idx, int nnodes, nodeoverview_line_t *line)
{
	node_t *node;
	meminfo_t meminfo;
	
	(void) memset(line, 0, sizeof (nodeoverview_line_t));
	if ((node = node_valid_get(nid_idx)) == NULL) {
		return;
	}

	node_meminfo(node->nid, &meminfo);
	
	node_win_countvalue(node, &line->value);
	line->nid = node->nid;
	line->mem_all = (double)((double)(meminfo.mem_total) /
		(double)(GB_BYTES));
	line->mem_free = (double)((double)(meminfo.mem_free) /
		(double)(GB_BYTES));
}

static boolean_t
nodeoverview_data_show(dyn_win_t *win, boolean_t *note_out)
{
	dyn_nodeoverview_t *dyn;
	win_reg_t *reg;
	char content[WIN_LINECHAR_MAX], intval_buf[16];
	int i, nnodes;
	nodeoverview_line_t *lines;

	*note_out = B_FALSE;
	dyn = (dyn_nodeoverview_t *)(win->dyn);

	disp_intval(intval_buf, 16);
	(void) snprintf(content, sizeof (content),
		"Node Overview (interval: %s)", intval_buf);

	reg = &dyn->msg;
	reg_erase(reg);
	reg_line_write(reg, 1, ALIGN_LEFT, content);
	reg_refresh_nout(reg);
	dump_write("\n*** %s\n", content);

	/*
	 * Display the caption of table:
	 * "NODE MEM.ALL MEM.FREE RMA(K) LMA(K) CPU%
	 */
	nodeoverview_caption_build(content, sizeof (content));
	reg = &dyn->caption_cur;
	reg_erase(reg);
	reg_line_write(reg, 1, ALIGN_LEFT, content);
	dump_write("%s\n", content);
	reg_refresh_nout(reg);

	nnodes = node_num();
	reg = &dyn->data_cur;
	reg_erase(reg);
	lines = (nodeoverview_line_t *)(reg->buf);
	reg->nlines_total = nnodes;

	/*
	 * Save the per-node data with metrics in scrolling buffer.
	 */
	for (i = 0; i < nnodes; i++) {
		nodeoverview_data_save(i, nnodes, &lines[i]);
	}

	/*
	 * Display the per-node data in scrolling buffer 
	 */
	reg_scroll_show(reg, (void *)lines, nnodes, nodeoverview_str_build);
	reg_refresh_nout(reg);

	/*
	 * Dispaly hint message for window type "WIN_TYPE_NODE_OVERVIEW"
	 */
	reg = &dyn->hint;
	reg_erase(reg);
	reg_line_write(reg, reg->nlines_scr - 2, ALIGN_LEFT,
	    "CPU%% = per-node CPU utilization");
	reg_refresh_nout(reg);

	return (B_TRUE);
}

/*
 * Display window on screen.
 * (window type: "WIN_TYPE_NODE_OVERVIEW")
 */
static boolean_t
nodeoverview_win_draw(dyn_win_t *win)
{
	boolean_t note_out, ret;

	title_show();
	node_group_lock();
	ret = nodeoverview_data_show(win, &note_out);
	node_group_unlock();

	if (!note_out) {
		note_show(NOTE_NODEOVERVIEW);
	}

	reg_update_all();
	return (ret);
}

/*
 * The function would be called when user hits the <UP>/<DOWN> key
 * to scroll data line.
 * (window type: "WIN_TYPE_NODE_OVERVIEW") 
 */
static void
nodeoverview_win_scroll(dyn_win_t *win, int scroll_type)
{
	dyn_nodeoverview_t *dyn = (dyn_nodeoverview_t *)(win->dyn);

	reg_line_scroll(&dyn->data_cur, scroll_type);
}

/*
 * The function would be called when user hits the "ENTER" key
 * on selected data line.
 * (window type: "WIN_TYPE_NODE_OVERVIEW") 
 */
static void
nodeoverview_win_scrollenter(dyn_win_t *win)
{
	dyn_nodeoverview_t *dyn = (dyn_nodeoverview_t *)(win->dyn);
	win_reg_t *reg = &dyn->data_cur;
	scroll_line_t *scroll = &reg->scroll;
	nodeoverview_line_t *lines;
	cmd_node_detail_t cmd;
	boolean_t badcmd;

	if (scroll->highlight == -1) {
		return;
	}

	/*
	 * Construct a command to switch to next window
	 * "WIN_TYPE_NODE_DETAIL".
	 */
	lines = (nodeoverview_line_t *)(reg->buf);
	cmd.id = CMD_NODE_DETAIL_ID;
	cmd.nid = lines[scroll->highlight].nid;	

	/* LINTED E_BAD_PTR_CAST_ALIGN */
	cmd_execute((cmd_t *)(&cmd), &badcmd);
}

/*
 * Initialize the display layout for window type
 * "WIN_TYPE_NODE_DETAIL"
 */
static dyn_nodedetail_t *
nodedetail_dyn_create(page_t *page)
{
	dyn_nodedetail_t *dyn;
	node_t *node;
	int i;

	if ((dyn = zalloc(sizeof (dyn_nodedetail_t))) == NULL) {
		return (NULL);
	}
	
	dyn->nid = CMD_NODE_DETAIL(&page->cmd)->nid;	
	node = node_get(dyn->nid);
	if (!NODE_VALID(node)) {
		free(dyn);
		win_warn_msg(WARN_INVALID_NID);
		return (NULL);	
	}

	i = reg_init(&dyn->msg, 0, 1, g_scr_width, 2, A_BOLD | A_UNDERLINE);
	i = reg_init(&dyn->node_data, 0, i, g_scr_width, g_scr_height - i - 4, 0);
	reg_init(&dyn->hint, 0, i, g_scr_width, 3, A_BOLD);
	return (dyn);
}

/*
 * Build a readable string of CPU ID and try to reduce the string length. e.g.
 * For cpu1, cpu2, cpu3, cpu4, the string is "CPU(1-4)",
 * For cpu1, cpu3, cpu5, cpu7, the string is "CPU(1 3 5 7)"
 */
static void
node_cpu_string(node_t *node, char *s1, int size)
{
	char s2[128];
	int j, l, cpuid_start;
	perf_cpu_t *cpus = node_cpus(node);

	if (node->ncpus == 0) {
		strncpy(s1, "-", size);
		s1[size - 1] = 0;
	} else {
		s1[0] = 0;
	}

	cpuid_start = cpus[0].cpuid;
	l = 1;
	for (j = 1; j < node->ncpus; j++) {
		if (cpus[j].cpuid != cpuid_start + l) {
			if (j < node->ncpus - 1) {
				if (l == 1) {
					(void) snprintf(s2, sizeof (s2), "%d ", cpuid_start);
				} else {
					(void) snprintf(s2, sizeof (s2),
						"%d-%d ", cpuid_start, cpuid_start + l - 1);
				}
          	} else {
				if (l == 1) {
					(void) snprintf(s2, sizeof (s2), "%d %d",
						cpuid_start, cpus[j].cpuid);
				} else {
					(void) snprintf(s2, sizeof (s2), "%d-%d",
						cpuid_start, cpuid_start + l - 1);
				}
			}

          	(void) strncat(s1, s2, strlen(s2));
          	cpuid_start = cpus[j].cpuid;
           	l = 1;
		} else {
        	if (j == node->ncpus - 1) {
            	(void) snprintf(s2, sizeof (s2), "%d-%d",
                	cpuid_start, cpuid_start + l);
         		(void) strncat(s1, s2, strlen(s2));
       		} else {
            	l++;
       		}
       	}
	}	
}

static void
nodedetail_line_show(win_reg_t *seg, char *title, char *value, int line)
{
	char s1[256];

	snprintf(s1, sizeof (s1), "%-20s%15s", title, value);
	reg_line_write(seg, line, ALIGN_LEFT, s1);
	dump_write("%s\n", s1);
}

/*
 * Display the performance statistics per node.
 */
static void
nodedetail_data(dyn_nodedetail_t *dyn, win_reg_t *seg)
{
	char s1[256];
	node_t *node;
	win_countvalue_t value;
	meminfo_t meminfo;
	int i = 1;

	reg_erase(seg);
	node = node_get(dyn->nid);
	node_win_countvalue(node, &value);
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

	reg_refresh_nout(seg);
}

static boolean_t
nodedetail_data_show(dyn_win_t *win, boolean_t *note_out)
{
	dyn_nodedetail_t *dyn;
	win_reg_t *reg;
	char content[WIN_LINECHAR_MAX], intval_buf[16];

	*note_out = B_FALSE;
	dyn = (dyn_nodedetail_t *)(win->dyn);

	/*
	 * Convert the sampling interval (nanosecond) to a human readable string.
	 */
	disp_intval(intval_buf, 16);
	(void) snprintf(content, sizeof (content),
		"Node%d information (interval: %s)", dyn->nid, intval_buf);

	reg = &dyn->msg;
	reg_erase(reg);
	reg_line_write(reg, 1, ALIGN_LEFT, content);
	reg_refresh_nout(reg);
	dump_write("\n*** %s\n", content);

	nodedetail_data((dyn_nodedetail_t *)(win->dyn), &dyn->node_data);

	reg = &dyn->hint;
 	reg_erase(reg);
	reg_line_write(reg, 1, ALIGN_LEFT,
	    "CPU%% = per-node CPU utilization");
	reg_refresh_nout(reg);

	return (B_FALSE);
}

/*
 * Display window on screen.
 * (window type: "WIN_TYPE_NODE_DETAIL")
 */
static boolean_t
nodedetail_win_draw(dyn_win_t *win)
{
	boolean_t note_out = B_FALSE, ret;

	title_show();
	node_group_lock();
	ret = nodedetail_data_show(win, &note_out);
	node_group_unlock();
	if (!note_out) {
		note_show(NOTE_NODEDETAIL);
	}

	reg_update_all();
	return (ret);
}

/*
 * Release the resources for window type "WIN_TYPE_NODE_DETAIL"
 */
static void
nodedetail_win_destroy(dyn_win_t *win)
{
	dyn_nodedetail_t *dyn;

	if ((dyn = win->dyn) != NULL) {
		reg_win_destroy(&dyn->msg);
		reg_win_destroy(&dyn->node_data);
		reg_win_destroy(&dyn->hint);
		free(dyn);
	}
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

static void
callchain_line_get(win_reg_t *reg, int idx, char *line, int size)
{
	callchain_line_t *lines;

	lines = (callchain_line_t *)(reg->buf);
	callchain_str_build(line, size, idx, (void *)lines);
}

/*
 * Initialize the display layout.
 * (window type: "WIN_TYPE_CALLCHAIN")
 */
static dyn_callchain_t *
callchain_dyn_create(page_t *page)
{
	dyn_callchain_t *dyn;
	cmd_callchain_t *cmd_callchain = CMD_CALLCHAIN(&page->cmd);
	int i;

	if ((dyn = zalloc(sizeof (dyn_callchain_t))) == NULL) {
		return (NULL);
	}

	dyn->pid = cmd_callchain->pid;
	dyn->lwpid = cmd_callchain->lwpid;
	dyn->keeprun_id = COUNT_RMA;

	i = reg_init(&dyn->msg, 0, 1, g_scr_width, 2, A_BOLD);
	i = reg_init(&dyn->caption, 0, i, g_scr_width, 2, A_BOLD | A_UNDERLINE);
	i = reg_init(&dyn->pad, 0, i, g_scr_width, 1, 0);
	i = reg_init(&dyn->data, 0, i, g_scr_width, g_scr_height - i - 4, 0);
	reg_buf_init(&dyn->data, NULL, callchain_line_get);
	reg_scroll_init(&dyn->data, B_TRUE);
	reg_init(&dyn->hint, 0, i, g_scr_width, g_scr_height - i - 1, A_BOLD);
	return (dyn);
}

/*
 * Release the resources of window.
 * (window type: "WIN_TYPE_CALLCHAIN")
 */
static void
callchain_win_destroy(dyn_win_t *win)
{
	dyn_callchain_t *dyn;

	if ((dyn = win->dyn) != NULL) {
		if (dyn->data.buf != NULL) {
			free(dyn->data.buf);
		}

		reg_win_destroy(&dyn->msg);
		reg_win_destroy(&dyn->caption);
		reg_win_destroy(&dyn->pad);
		reg_win_destroy(&dyn->data);
		reg_win_destroy(&dyn->hint);
		free(dyn);
	}
}

static void
callchain_keeprun_event(count_id_t count_id, char *buf, int size)
{
	switch (count_id) {
	case COUNT_RMA:
		strncpy(buf, "RMA", size);
		break;
	
	case COUNT_CLK:
		strncpy(buf, "Cycle", size);
		break;
	
	case COUNT_IR:
		strncpy(buf, "IR", size);
		break;
		
	case COUNT_LMA:
		strncpy(buf, "LMA", size);
		break;
		
	default:
		strncpy(buf, "-", size);
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

static int
callchain_list_show(dyn_callchain_t *dyn, track_proc_t *proc,
	track_lwp_t *lwp)
{
	perf_countchain_t *count_chain;
	perf_chainrecgrp_t *rec_grp;
	perf_chainrec_t *rec_arr;
	sym_chainlist_t chainlist;
	win_reg_t *reg;
	char content[WIN_LINECHAR_MAX];
	int i;

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
	rec_grp = &count_chain->chaingrps[dyn->keeprun_id];
	rec_arr = rec_grp->rec_arr;
	
	for (i = 0; i < rec_grp->nrec_cur; i++) {
		sym_callchain_add(&proc->sym, rec_arr[i].callchain.ips,
			rec_arr[i].callchain.ip_num, &chainlist);		
	}

	chainlist_show(&chainlist, &dyn->data);
	return (0);
}

static void
callchain_data_show(dyn_win_t *win, boolean_t *note_out)
{
	dyn_callchain_t *dyn;
	pid_t pid;
	int lwpid;
	track_proc_t *proc;
	track_lwp_t *lwp = NULL;
	win_reg_t *reg;
	char content[WIN_LINECHAR_MAX], event_name[32], intval_buf[16];

	dyn = (dyn_callchain_t *)(win->dyn);
	pid = dyn->pid;
	lwpid = dyn->lwpid;
	*note_out = B_FALSE;
	
	if ((proc = proc_find(pid)) == NULL) {
		win_warn_msg(WARN_INVALID_PID);
		note_show(NOTE_INVALID_PID);
		*note_out = B_TRUE;
		return;
	}

	if ((lwpid > 0) && ((lwp = proc_lwp_find(proc, lwpid)) == NULL)) {
		proc_refcount_dec(proc);
		win_warn_msg(WARN_INVALID_LWPID);
		note_show(NOTE_INVALID_LWPID);
		*note_out = B_TRUE;
		return;
	}

	reg = &dyn->msg;
	reg_erase(reg);
	callchain_keeprun_event(dyn->keeprun_id, event_name, 32);
	disp_intval(intval_buf, 16);
	if (lwpid == 0) {
		(void) snprintf(content, WIN_LINECHAR_MAX,
			"Call-chain when process generates "
	    	"\"%s\" (pid: %d, interval: %s)",
			event_name, pid, intval_buf);
	} else {
		(void) snprintf(content, WIN_LINECHAR_MAX,
	    	"Call-chain when thread generates "
	    	"\"%s\" (lwpid: %d, interval: %s)",
			event_name, lwpid, intval_buf);
	}

	dump_write("\n*** %s\n", content);
	reg_line_write(reg, 1, ALIGN_LEFT, content);
	reg_refresh_nout(reg);

	/*
	 * Display the call-chain.
	 */
	callchain_list_show(dyn, proc, lwp);

	/*
	 * Display hint message for window type "WIN_TYPE_CALLCHAIN"
	 */
	reg = &dyn->hint;
	reg_erase(reg);
	reg_line_write(reg, 1, ALIGN_LEFT,
		"Switch call-chain by: 1(RMA), 2(LMA), 3(CYCLE), 4(IR)");
	reg_refresh_nout(reg);
	
	if (lwp != NULL) {
		lwp_refcount_dec(lwp);
	}
	
	proc_refcount_dec(proc);
}

/*
 * Display window on screen.
 * (window type: "WIN_TYPE_CALLCHAIN")
 */
static boolean_t
callchain_win_draw(dyn_win_t *win)
{
	boolean_t note_out;

	title_show();
	callchain_data_show(win, &note_out);
	if (!note_out) {
		note_show(NOTE_CALLCHAIN);
	}

	reg_update_all();
	return (B_TRUE);
}

/*
 * The callback function for "WIN_TYPE_CALLCHAIN" would be called
 * when user hits the <UP>/<DOWN> key to scroll data line.
 */
static void
callchain_win_scroll(dyn_win_t *win, int scroll_type)
{
	dyn_callchain_t *dyn = (dyn_callchain_t *)(win->dyn);

	reg_line_scroll(&dyn->data, scroll_type);
}

static void
size2str(uint64_t size, char *buf, int bufsize)
{
	uint64_t i, j;

	/*
	 * "buf" points to a big enough buffer.
	 */
	if ((i = (size / KB_BYTES)) < KB_BYTES) {
		(void) snprintf(buf, bufsize, "%luK", i);
	} else if ((j = i / KB_BYTES) < KB_BYTES) {
		(void) snprintf(buf, bufsize, "%luM", j);
	} else {
		(void) snprintf(buf, bufsize, "%luG", j / KB_BYTES);
	}
}

/*
 * Build the readable string of data line which contains buffer address,
 * buffer size, access%, latency (nanosecond) and buffer description.
 */
static void
lat_str_build(char *buf, int size, int idx, void *pv)
{
	lat_line_t *lines = (lat_line_t *)pv;
	lat_line_t *line = &lines[idx];
	float hit = 0.0;
	int lat = 0;
	char size_str[32];

	if (line->nsamples > 0) {
		hit = (float)(line->naccess) / (float)(line->nsamples);
	}

	if (line->naccess > 0) {
		lat = (line->latency) / (line->naccess);
	}

	size2str(line->bufaddr.size, size_str, sizeof (size_str));

	if (!line->nid_show) {
		snprintf(buf, size, "%16lX%8s%10.1f%11lu%34s",
		    line->bufaddr.addr, size_str, hit * 100.0, cyc2ns(lat),
		    line->desc);
	} else {
		if (line->nid < 0) {
			snprintf(buf, size, "%16lX%8s%8s%10.1f%11lu",
			    line->bufaddr.addr, size_str, "-", hit * 100.0, cyc2ns(lat));
		} else {
			snprintf(buf, size, "%16lX%8s%8d%10.1f%11lu",
			    line->bufaddr.addr, size_str, line->nid, hit * 100.0,
			    cyc2ns(lat));
		}		
	}
}

static void
lat_line_get(win_reg_t *reg, int idx, char *line, int size)
{
	lat_line_t *lines;

	lines = (lat_line_t *)(reg->buf);
	lat_str_build(line, size, idx, (void *)lines);
}

/*
 * Initialize the display layout for window type
 * "WIN_TYPE_LAT_PROC" and "WIN_TYPE_LAT_LWP"
 */
static void *
lat_dyn_create(page_t *page, win_type_t *type)
{
	dyn_lat_t *dyn;
	cmd_lat_t *cmd_lat = CMD_LAT(&page->cmd);
	int i;

	if ((dyn = zalloc(sizeof (dyn_lat_t))) == NULL) {
		return (NULL);
	}

	i = reg_init(&dyn->msg, 0, 1, g_scr_width, 2, A_BOLD);
	i = reg_init(&dyn->caption, 0, i, g_scr_width, 2, A_BOLD | A_UNDERLINE);
	reg_init(&dyn->data, 0, i, g_scr_width, g_scr_height - i - 2, 0);

	reg_buf_init(&dyn->data, NULL, lat_line_get);
	reg_scroll_init(&dyn->data, B_TRUE);

	dyn->pid = cmd_lat->pid;
	if ((dyn->lwpid = cmd_lat->lwpid) != 0) {
		*type = WIN_TYPE_LAT_LWP;
	} else {
		*type = WIN_TYPE_LAT_PROC;
	}

	return (dyn);
}

/*
 * Release the resources for window type
 * "WIN_TYPE_LAT_PROC" and "WIN_TYPE_LAT_LWP"
 */
static void
lat_win_destroy(dyn_win_t *win)
{
	dyn_lat_t *dyn;

	if ((dyn = win->dyn) != NULL) {
		if (dyn->data.buf != NULL) {
			free(dyn->data.buf);
		}

		reg_win_destroy(&dyn->msg);
		reg_win_destroy(&dyn->caption);
		reg_win_destroy(&dyn->data);
		free(dyn);
	}
}

/*
 * Due to the limitation of screen size, the string of path
 * probablly needs to be cut. For example:
 * /export/home/jinyao/ws/numatop-gate/usr/src/cmd/numatop/amd64/numatop
 * probably is cut to:
 * ../usr/src/cmd/numatop/amd64/numatop
 */
void
bufdesc_cut(char *dst_desc, int dst_size, char *src_desc)
{
	int src_len;
	char *start, *end;

	if ((src_len = strlen(src_desc)) < dst_size) {
		strcpy(dst_desc, src_desc);
		if ((src_len == 0) && (dst_size > 0)) {
			dst_desc[0] = 0;
		}

		return;
	}

	end = src_desc + src_len;
	start = end - dst_size;

	while ((start < end) && (*start != '/')) {
		start++;
	}

	if (start < end) {
		snprintf(dst_desc, dst_size, "..%s", start);
	} else {
		strncpy(dst_desc, src_desc, dst_size);
		dst_desc[dst_size - 1] = 0;
	}
}

/*
 * copyout the maps data to a new buffer.
 */
static lat_line_t *
lat_buf_create(track_proc_t *proc, int lwpid, int *nlines)
{
	map_t *map = &proc->map;
	map_entry_t *entry;
	lat_line_t *buf;
	int i;

	*nlines = map->nentry_cur;
	if ((buf = zalloc(sizeof (lat_line_t) * (*nlines))) == NULL) {
		return (NULL);
	}

	for (i = 0; i < *nlines; i++) {
		entry = &map->arr[i];		
		buf[i].pid = proc->pid;
		buf[i].lwpid = lwpid;
		buf[i].bufaddr.addr = entry->start_addr;
		buf[i].bufaddr.size = entry->end_addr - entry->start_addr;
		buf[i].nid_show = B_FALSE;
		bufdesc_cut(buf[i].desc, WIN_DESCBUF_SIZE, entry->desc);
	}

	return (buf);
}

/*
 * The callback function used in bsearch() to compare the linear address.
 */
static int
bufaddr_cmp(const void *p1, const void *p2)
{
	uint64_t addr = (uint64_t)p1;
	bufaddr_t *bufaddr = (bufaddr_t *)p2;

	if (addr < bufaddr->addr) {
		return (-1);
	}

	if (addr >= bufaddr->addr + bufaddr->size) {
		return (1);
	}

	return (0);
}

/*
 * "lat_buf" points to an array which contains the process address mapping.
 * Each item in array represents a buffer in process address space. "rec"
 * points to a SMPL record. The function is responsible for checking and
 * comparing the record with the process address mapping.
 */
static void
lat_buf_hit(lat_line_t *lat_buf, int nlines, perf_llrec_t *rec,
	uint64_t *total_lat, uint64_t *total_sample)
{
	lat_line_t *line;
	
	/*
	 * Check if the linear address is located in a buffer in
	 * process address space.
	 */
	if ((line = bsearch((void *)(rec->addr), lat_buf, nlines,
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

/*
 * Get the LL sampling data, check if the record hits one buffer in
 * process address space. If so, update the accessing statistics for
 * this buffer.
 */
static void
lat_buf_fill(lat_line_t *lat_buf, int nlines, track_proc_t *proc,
    track_lwp_t *lwp, int *lat)
{
	perf_llrecgrp_t *grp;
	perf_llrec_t *rec;
	uint64_t total_sample = 0, total_lat = 0;
	int i;
	
	if (lwp == NULL) {
		grp = &proc->llrec_grp;
	} else {
		grp = &lwp->llrec_grp;
	}

	for (i = 0; i < grp->nrec_cur; i++) {
		rec = &grp->rec_arr[i];
		lat_buf_hit(lat_buf, nlines, rec, &total_lat, &total_sample);
	}
	
	for (i = 0; i < nlines; i++) {
		lat_buf[i].nsamples = total_sample;		
	}

	*lat = (total_sample > 0) ? (total_lat / total_sample) : 0;		
}

/*
 * The callback function used in qsort() to compare the number of 
 * buffer accessing.
 */
static int
lat_cmp(const void *p1, const void *p2)
{
	lat_line_t *l1 = (lat_line_t *)p1;
	lat_line_t *l2 = (lat_line_t *)p2;

	if (l1->naccess < l2->naccess) {
		return (1);
	}

	if (l1->naccess > l2->naccess) {
		return (-1);
	}

	return (0);
}

/*
 * Get and display the process/thread latency related information.
 */
static int
lat_data_get(track_proc_t *proc, track_lwp_t *lwp, dyn_lat_t *dyn, int *lat)
{
	lat_line_t *lat_buf;
	int nlines, lwpid = 0;
	char content[WIN_LINECHAR_MAX];

	reg_erase(&dyn->caption);
	reg_refresh_nout(&dyn->caption);
	reg_erase(&dyn->data);
	reg_refresh_nout(&dyn->data);

	if (lwp != NULL) {
		lwpid = lwp->id;		
	}

	if ((lat_buf = lat_buf_create(proc, lwpid, &nlines)) == NULL) {
		debug_print(NULL, 2, "lat_buf_create failed (pid = %d)\n", proc->pid);
		return (-1);
	}

	/*
	 * Fill in the memory access information.
	 */
	lat_buf_fill(lat_buf, nlines, proc, lwp, lat);

	/*
	 * Sort the "lat_buf" according to the number of buffer accessing.
	 */
	qsort(lat_buf, nlines, sizeof (lat_line_t), lat_cmp);

	/*
	 * Display the caption of data table:
	 * "ADDR SIZE ACCESS% LAT(ns) DESC"
	 */
	(void) snprintf(content, sizeof (content),
	    "%16s%8s%11s%11s%34s",
	    CAPTION_ADDR, CAPTION_SIZE, CAPTION_BUFHIT,
	    CAPTION_AVGLAT, CAPTION_DESC);

	reg_line_write(&dyn->caption, 1, ALIGN_LEFT, content);
	dump_write("%s\n", content);
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
	dyn->data.buf = (void *)lat_buf;
	reg_scroll_show(&dyn->data, (void *)(dyn->data.buf),
	    nlines, lat_str_build);
	reg_refresh_nout(&dyn->data);

	return (0);
}

static boolean_t
lat_data_show(track_proc_t *proc, dyn_lat_t *dyn, boolean_t *note_out)
{
	win_reg_t *reg;
	int lat;
	track_lwp_t *lwp = NULL;
	char content[WIN_LINECHAR_MAX], intval_buf[16];	

	*note_out = B_FALSE;
	if ((dyn->lwpid != 0) &&
	    (lwp = proc_lwp_find(proc, dyn->lwpid)) == NULL) {
		win_warn_msg(WARN_INVALID_LWPID);
		note_show(NOTE_INVALID_LWPID);
		*note_out = B_TRUE;
		return (B_FALSE);
	}

	dump_cache_enable();
	(void) lat_data_get(proc, lwp, dyn, &lat);
	dump_cache_disable();

	disp_intval(intval_buf, 16);
	if (lwp == NULL) {
		(void) snprintf(content, sizeof (content),
		    "Monitoring memory areas (pid: %d, "
		    "AVG.LAT: %luns, interval: %s)", proc->pid, cyc2ns(lat), intval_buf);
	} else {
		(void) snprintf(content, sizeof (content),
		    "Monitoring memory areas (lwpid: %d, "
		    "AVG.LAT: %luns, interval: %s)",
		    lwp->id, cyc2ns(lat), intval_buf);
	}

	reg = &dyn->msg;
	reg_erase(reg);
	reg_line_write(reg, 1, ALIGN_LEFT, content);
	dump_write("\n*** %s\n", content);
	reg_refresh_nout(reg);

	dump_cache_flush();

	if (lwp != NULL) {
		lwp_refcount_dec(lwp);
	}

	return (B_TRUE);
}

/*
 * The implementation of displaying window on screen for
 * window type "WIN_TYPE_LAT_PROC" and "WIN_TYPE_LAT_LWP"
 */
static boolean_t
lat_win_draw(dyn_win_t *win)
{
	dyn_lat_t *dyn = (dyn_lat_t *)(win->dyn);
	track_proc_t *proc;
	boolean_t note_out, ret;

	if (!perf_ll_started()) {		
		win_warn_msg(WARN_LL_NOT_SUPPORT);
		note_show(NOTE_LAT);
		return (B_FALSE);
	}

	if ((proc = proc_find(dyn->pid)) == NULL) {
		win_warn_msg(WARN_INVALID_PID);
		note_show(NOTE_INVALID_PID);
		return (B_FALSE);
	}

	if (map_load(proc) != 0) {
		proc_refcount_dec(proc);
		win_warn_msg(WARN_INVALID_MAP);
		note_show(NOTE_INVALID_MAP);
		return (B_FALSE);
	}

	title_show();
	ret = lat_data_show(proc, dyn, &note_out);
	if (!note_out) {
		note_show(NOTE_LAT);
	}

	proc_refcount_dec(proc);
	reg_update_all();
	return (ret);
}

/*
 * The callback function for "WIN_TYPE_LAT_PROC" and "WIN_TYPE_LAT_LWP"
 * would be called when user hits the <UP>/<DOWN> key to scroll data line.
 */
static void
lat_win_scroll(dyn_win_t *win, int scroll_type)
{
	dyn_lat_t *dyn = (dyn_lat_t *)(win->dyn);

	reg_line_scroll(&dyn->data, scroll_type);
}

/*
 * The callback function for window type "WIN_TYPE_LAT_PROC"
 * and "WIN_TYPE_LAT_LWP" would be called when user hits the "ENTER" key
 * on selected data line.
 */
static void
lat_win_scrollenter(dyn_win_t *win)
{
	dyn_lat_t *dyn = (dyn_lat_t *)(win->dyn);
	win_reg_t *reg = &dyn->data;
	scroll_line_t *scroll = &reg->scroll;
	lat_line_t *lines;
	cmd_latnode_t cmd;
	boolean_t badcmd;

	if (scroll->highlight == -1) {
		return;
	}

	/*
	 * Construct a command to switch to next window
	 * "WIN_TYPE_LATNODE_PROC" / "WIN_TYPE_LATNODE_LWP"
	 */
	lines = (lat_line_t *)(reg->buf);
	cmd.id = CMD_LATNODE_ID;
	cmd.pid = lines[scroll->highlight].pid;
	cmd.lwpid = lines[scroll->highlight].lwpid;
	cmd.addr = lines[scroll->highlight].bufaddr.addr;
	cmd.size = lines[scroll->highlight].bufaddr.size;

	cmd_execute((cmd_t *)(&cmd), &badcmd);
}

/*
 * The callback function for "WIN_TYPE_LATNODE_PROC" and "WIN_TYPE_LATNODE_LWP"
 * would be called when user hits the <UP>/<DOWN> key to scroll data line.
 */
static void
latnode_win_scroll(dyn_win_t *win, int scroll_type)
{
	dyn_latnode_t *dyn = (dyn_latnode_t *)(win->dyn);

	reg_line_scroll(&dyn->data, scroll_type);
}

/*
 * Initialize the display layout for window type
 * "WIN_TYPE_LATNODE_PROC" and "WIN_TYPE_LATNODE_LWP"
 */
static void *
latnode_dyn_create(page_t *page, win_type_t *type)
{
	dyn_latnode_t *dyn;
	cmd_latnode_t *cmd = CMD_LATNODE(&page->cmd);
	int i;

	if ((dyn = zalloc(sizeof (dyn_latnode_t))) == NULL) {
		return (NULL);
	}

	i = reg_init(&dyn->msg, 0, 1, g_scr_width, 2, A_BOLD);
	i = reg_init(&dyn->note, 0, i, g_scr_width, 2, A_BOLD);
	i = reg_init(&dyn->caption, 0, i, g_scr_width, 2, A_BOLD | A_UNDERLINE);
	reg_init(&dyn->data, 0, i, g_scr_width, g_scr_height - i - 2, 0);

	reg_buf_init(&dyn->data, NULL, lat_line_get);
	reg_scroll_init(&dyn->data, B_TRUE);

	dyn->pid = cmd->pid;
	if ((dyn->lwpid = cmd->lwpid) != 0) {
		*type = WIN_TYPE_LATNODE_LWP;
	} else {
		*type = WIN_TYPE_LATNODE_PROC;
	}

	dyn->addr = cmd->addr;
	dyn->size = cmd->size;
	return (dyn);
}

/*
 * Release the resources for window type
 * "WIN_TYPE_LATNODE_PROC" and "WIN_TYPE_LATNODE_LWP"
 */
static void
latnode_win_destroy(dyn_win_t *win)
{
	dyn_latnode_t *dyn;

	if ((dyn = win->dyn) != NULL) {
		if (dyn->data.buf != NULL) {
			free(dyn->data.buf);
		}

		reg_win_destroy(&dyn->msg);
		reg_win_destroy(&dyn->note);
		reg_win_destroy(&dyn->caption);
		reg_win_destroy(&dyn->data);
		free(dyn);
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

	lat_buf_fill(buf, nlines, proc, lwp, &lat);

	/*
	 * Sort by the number of buffer accessing.
	 */
	qsort(buf, nlines, sizeof (lat_line_t), lat_cmp);

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
	    nlines, lat_str_build);
	reg_refresh_nout(&dyn->data);

	return (0);
}

static boolean_t
latnode_data_show(track_proc_t *proc, dyn_latnode_t *dyn, map_entry_t *entry,
	boolean_t *note_out)
{
	win_reg_t *reg;
	track_lwp_t *lwp = NULL;
	char content[WIN_LINECHAR_MAX], intval_buf[16], size_str[32];

	*note_out = B_FALSE;

	if ((dyn->lwpid != 0) &&
	    (lwp = proc_lwp_find(proc, dyn->lwpid)) == NULL) {
		win_warn_msg(WARN_INVALID_LWPID);
		note_show(NOTE_INVALID_LWPID);
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
	size2str(dyn->size, size_str, sizeof (size_str));
	if (lwp != NULL) {
		(void) snprintf(content, sizeof (content),
		    "Memory area(%lX, %s), thread(%d)",
		    dyn->addr, size_str, lwp->id);
	} else {
		(void) snprintf(content, sizeof (content),
		    "Memory area(%lX, %s), process(%d)",
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
static boolean_t
latnode_win_draw(dyn_win_t *win)
{
	dyn_latnode_t *dyn = (dyn_latnode_t *)(win->dyn);
	track_proc_t *proc;
	map_entry_t *entry;
	boolean_t note_out, ret;

	if ((proc = proc_find(dyn->pid)) == NULL) {
		win_warn_msg(WARN_INVALID_PID);
		note_show(NOTE_INVALID_PID);
		return (B_FALSE);
	}

	if ((entry = map_entry_find(proc, dyn->addr, dyn->size)) == NULL) {
		proc_refcount_dec(proc);
		win_warn_msg(WARN_INVALID_MAP);
		note_show(NOTE_INVALID_MAP);
		return (B_FALSE);
	}

	if (map_map2numa(proc, entry) != 0) {
		proc_refcount_dec(proc);
		win_warn_msg(WARN_INVALID_NUMAMAP);
		note_show(NOTE_INVALID_NUMAMAP);
		return (B_FALSE);		
	}

	title_show();
	ret = latnode_data_show(proc, dyn, entry, &note_out);
	if (!note_out) {
		note_show(NOTE_LATNODE);
	}

	proc_refcount_dec(proc);
	reg_update_all();
	return (ret);
}

/*
 * Build the readable string for data line.
 * (window type: "WIN_TYPE_ACCDST_PROC" and "WIN_TYPE_ACCDST_LWP")
 */
static void
accdst_str_build(char *buf, int size, int idx, void *pv)
{
	accdst_line_t *lines = (accdst_line_t *)pv;
	accdst_line_t *line = &lines[idx];

	snprintf(buf, size, "%5d%14.1f%15lu",
		line->nid, line->access_ratio * 100.0, cyc2ns(line->latency));
}

/*
 * Build the readable string for scrolling line.
 * (window type: "WIN_TYPE_ACCDST_PROC" and "WIN_TYPE_ACCDST_LWP")
 */
static void
accdst_line_get(win_reg_t *reg, int idx, char *line, int size)
{
	accdst_line_t *lines;

	lines = (accdst_line_t *)(reg->buf);
	accdst_str_build(line, size, idx, (void *)lines);
}

/*
 * The callback function for "WIN_TYPE_ACCDST_PROC" and "WIN_TYPE_ACCDST_LWP"
 * would be called when user hits the <UP>/<DOWN> key to scroll data line.
 */
static void
accdst_win_scroll(dyn_win_t *win, int scroll_type)
{
	dyn_accdst_t *dyn = (dyn_accdst_t *)(win->dyn);

	reg_line_scroll(&dyn->data, scroll_type);
}

/*
 * Initialize the display layout for window type
 * "WIN_TYPE_ACCDST_PROC" and "WIN_TYPE_ACCDST_LWP"
 */
static void *
accdst_dyn_create(page_t *page, win_type_t *type)
{
	dyn_accdst_t *dyn;
	void *buf;
	int i, nnodes;
	cmd_accdst_t *cmd_accdst = CMD_ACCDST(&page->cmd);

	nnodes = node_num();
	if ((dyn = zalloc(sizeof (dyn_accdst_t))) == NULL) {
		return (NULL);
	}
	
	if ((buf = zalloc(sizeof (accdst_line_t) * nnodes)) == NULL) {
		free(dyn);
		return (NULL);
	}

	i = reg_init(&dyn->msg, 0, 1, g_scr_width, 2, A_BOLD);
	i = reg_init(&dyn->caption, 0, i, g_scr_width, 2, A_BOLD | A_UNDERLINE);
	i = reg_init(&dyn->data, 0, i, g_scr_width, nnodes, 0);
	reg_buf_init(&dyn->data, buf, accdst_line_get);
	reg_scroll_init(&dyn->data, B_TRUE);
	reg_init(&dyn->hint, 0, i, g_scr_width, g_scr_height - i - 1, A_BOLD);

	dyn->pid = cmd_accdst->pid;
	if ((dyn->lwpid = cmd_accdst->lwpid) != 0) {
		*type = WIN_TYPE_ACCDST_LWP;
	} else {
		*type = WIN_TYPE_ACCDST_PROC;
	}

	return (dyn);
}

/*
 * Release the resources for window type
 * "WIN_TYPE_ACCDST_PROC" and "WIN_TYPE_ACCDST_LWP"
 */
static void
accdst_win_destroy(dyn_win_t *win)
{
	dyn_accdst_t *dyn;

	if ((dyn = win->dyn) != NULL) {
		if (dyn->data.buf != NULL) {
			free(dyn->data.buf);
		}

		reg_win_destroy(&dyn->msg);
		reg_win_destroy(&dyn->caption);
		reg_win_destroy(&dyn->data);
		reg_win_destroy(&dyn->hint);
		free(dyn);
	}
}

static int
llrec2addr(track_proc_t *proc, track_lwp_t *lwp, void ***addr_arr,
	int **lat_arr, int *addr_num)
{
	perf_llrecgrp_t *grp;
	void **addr_buf;
	int *lat_buf;
	int i;
	
	if (lwp == NULL) {
		grp = &proc->llrec_grp;
	} else {
		grp = &lwp->llrec_grp;
	}
	
	if (grp->nrec_cur == 0) {
		*addr_arr = NULL;
		*lat_arr = NULL;
		*addr_num = 0;	
		return (0);
	}
	
	if ((addr_buf = zalloc(sizeof (void *) * grp->nrec_cur)) == NULL) {
		return (-1);
	}
	
	if ((lat_buf = zalloc(sizeof (int) * grp->nrec_cur)) == NULL) {
		free(addr_buf);
		return (-1);
	}
	
	for (i = 0; i < grp->nrec_cur; i++) {
		addr_buf[i] = (void *)(grp->rec_arr[i].addr);
		lat_buf[i] = grp->rec_arr[i].latency;		
	}
	
	*addr_arr = addr_buf;
	*lat_arr = lat_buf;
	*addr_num = grp->nrec_cur;
	return (0);
}

static void
accdst_data_save(map_nodedst_t *nodedst_arr, int nnodes_max, int naccess_total,
	int nid_idx, accdst_line_t *line)
{
	node_t *node;
	int naccess;
	
	memset(line, 0, sizeof (accdst_line_t));
	if ((node = node_valid_get(nid_idx)) == NULL) {
		return;
	}

	if ((node->nid < 0) || (node->nid >= nnodes_max)) {
		return;
	}
	
	line->nid = node->nid;
	naccess = nodedst_arr[node->nid].naccess;
	
	if (naccess_total > 0) {
		line->access_ratio = (double)naccess / (double)naccess_total;
	}

	if (naccess > 0) {
		line->latency = nodedst_arr[node->nid].total_lat / naccess;
	}
}

static boolean_t
accdst_data_show(track_proc_t *proc, dyn_accdst_t *dyn, boolean_t *note_out)
{
	win_reg_t *reg;
	track_lwp_t *lwp = NULL;
	void **addr_arr = NULL;
	int *lat_arr = NULL;
	int addr_num, i, nnodes, naccess_total;
	map_nodedst_t nodedst_arr[NNODES_MAX];
	accdst_line_t *lines;
	char content[WIN_LINECHAR_MAX], intval_buf[16];
	boolean_t ret = B_FALSE;

	*note_out = B_FALSE;

	if ((dyn->lwpid != 0) &&
	    (lwp = proc_lwp_find(proc, dyn->lwpid)) == NULL) {
		win_warn_msg(WARN_INVALID_LWPID);
		note_show(NOTE_INVALID_LWPID);
		*note_out = B_TRUE;
		return (B_FALSE);
	}

	if (llrec2addr(proc, lwp, &addr_arr, &lat_arr, &addr_num) != 0) {
		goto L_EXIT;
	}

	memset(nodedst_arr, 0, sizeof (map_nodedst_t) * NNODES_MAX);
	if (addr_num > 0) {
		if (map_addr2nodedst(proc->pid, addr_arr, lat_arr, addr_num,
			nodedst_arr, NNODES_MAX, &naccess_total) != 0) {
			goto L_EXIT;
		}
	}

	reg = &dyn->msg;
	reg_erase(reg);
	disp_intval(intval_buf, 16);

	if (lwp == NULL) {
		(void) snprintf(content, sizeof (content),
		    "Memory access node distribution overview (pid: %d, interval: %s)",
		    proc->pid, intval_buf);
	} else {
		(void) snprintf(content, sizeof (content),
		    "Memory access node distribution overview (lwpid: %d, interval: %s)",
		    lwp->id, intval_buf);
	}

	reg_line_write(reg, 1, ALIGN_LEFT, content);
	dump_write("\n*** %s\n", content);
	reg_refresh_nout(reg);		

	reg = &dyn->caption;
	reg_erase(reg);

	/*
	 * Display the caption of table:
	 * "NODE	ACCESS%		LAT(ns)"
	 */
	(void) snprintf(content, sizeof (content), "%5s%15s%15s",
	    CAPTION_NID, CAPTION_BUFHIT, CAPTION_AVGLAT);

	reg_line_write(reg, 1, ALIGN_LEFT, content);
	dump_write("%s\n", content);
	reg_refresh_nout(reg);

	nnodes = node_num();
	reg = &dyn->data;
	reg_erase(reg);
	lines = (accdst_line_t *)(reg->buf);
	reg->nlines_total = nnodes;

	/*
	 * Save the per-node data with metrics in scrolling buffer.
	 */
	for (i = 0; i < nnodes; i++) {
		accdst_data_save(nodedst_arr, NNODES_MAX, naccess_total, i,
			&lines[i]);
	}
	
	/*
	 * Display the per-node data in scrolling buffer 
	 */
	reg_scroll_show(reg, (void *)lines, nnodes, accdst_str_build);
	reg_refresh_nout(reg);
	
	reg = &dyn->hint;
	reg_erase(reg);
	reg_refresh_nout(reg);	

L_EXIT:
	if (lwp != NULL) {
		lwp_refcount_dec(lwp);
	}

	if (addr_arr != NULL) {
		free(addr_arr);
	}
	
	if (lat_arr != NULL) {
		free(lat_arr);
	}

	return (ret);
}

/*
 * The implementation of displaying window on screen for
 * window type "WIN_TYPE_ACCDST_PROC" and "WIN_TYPE_ACCDST_LWP"
 */
static boolean_t
accdst_win_draw(dyn_win_t *win)
{
	dyn_accdst_t *dyn = (dyn_accdst_t *)(win->dyn);
	track_proc_t *proc;
	boolean_t note_out, ret;

	if ((proc = proc_find(dyn->pid)) == NULL) {
		win_warn_msg(WARN_INVALID_PID);
		note_show(NOTE_INVALID_PID);
		return (B_FALSE);
	}

	title_show();
	ret = accdst_data_show(proc, dyn, &note_out);
	if (!note_out) {
		note_show(NOTE_ACCDST);
	}

	proc_refcount_dec(proc);
	reg_update_all();
	return (ret);
}

/*
 * Initialize the display layout.
 * (window type: "WIN_TYPE_LLCALLCHAIN")
 */
static dyn_llcallchain_t *
llcallchain_dyn_create(page_t *page)
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

	i = reg_init(&dyn->msg, 0, 1, g_scr_width, 2, A_BOLD);
	i = reg_init(&dyn->buf_caption, 0, i, g_scr_width, 2, A_BOLD | A_UNDERLINE);
	i = reg_init(&dyn->buf_data, 0, i, g_scr_width, 1, 0);
	i = reg_init(&dyn->chain_caption, 0, i, g_scr_width, 2, A_BOLD | A_UNDERLINE);
	i = reg_init(&dyn->pad, 0, i, g_scr_width, 1, 0);
	reg_init(&dyn->chain_data, 0, i, g_scr_width, g_scr_height - i - 2, 0);
	reg_buf_init(&dyn->chain_data, NULL, callchain_line_get);
	reg_scroll_init(&dyn->chain_data, B_TRUE);

	return (dyn);
}

/*
 * Release the resources of window.
 * (window type: "WIN_TYPE_LLCALLCHAIN")
 */
static void
llcallchain_win_destroy(dyn_win_t *win)
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
	 
	if ((lat_buf = lat_buf_create(proc, lwpid, &nlines)) == NULL) {
		return (-1);
	}

	/*
	 * Fill in the memory access information.
	 */
	lat_buf_fill(lat_buf, nlines, proc, lwp, &lat);
	
	/*
	 * Check if the linear address is located in a buffer in
	 * process address space.
	 */ 
	if ((line = bsearch((void *)(dyn->addr), lat_buf, nlines,
		sizeof (lat_line_t), bufaddr_cmp)) != NULL) {
		lat_str_build(content, WIN_LINECHAR_MAX, 0, line);
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
	perf_llrec_t *rec_arr;
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
		note_show(NOTE_INVALID_PID);
		*note_out = B_TRUE;
		return;
	}

	if ((lwpid > 0) && ((lwp = proc_lwp_find(proc, lwpid)) == NULL)) {
		proc_refcount_dec(proc);
		win_warn_msg(WARN_INVALID_LWPID);
		note_show(NOTE_INVALID_LWPID);
		*note_out = B_TRUE;
		return;
	}

	reg = &dyn->msg;
	reg_erase(reg);	
	disp_intval(intval_buf, 16);
	size2str(size, size_str, sizeof (size_str));

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
static boolean_t
llcallchain_win_draw(dyn_win_t *win)
{
	boolean_t note_out;

	title_show();
	llcallchain_data_show(win, &note_out);
	if (!note_out) {
		note_show(NOTE_LLCALLCHAIN);
	}

	reg_update_all();
	return (B_TRUE);
}

/*
 * The callback function for "WIN_TYPE_LLCALLCHAIN" would be called
 * when user hits the <UP>/<DOWN> key to scroll data line.
 */
static void
llcallchain_win_scroll(dyn_win_t *win, int scroll_type)
{
	dyn_llcallchain_t *dyn = (dyn_llcallchain_t *)(win->dyn);

	reg_line_scroll(&dyn->chain_data, scroll_type);
}

/*
 * The common entry for all warning messages.
 */
void
win_warn_msg(win_type_t warn_type)
{
	dyn_warn_t dyn;
	char content[WIN_LINECHAR_MAX];
	int i;

	i = reg_init(&dyn.msg, 0, 1, g_scr_width, 4, A_BOLD);
	reg_init(&dyn.pad, 0, i, g_scr_width, g_scr_height - i - 2, 0);		

	reg_erase(&dyn.pad);
	reg_line_write(&dyn.pad, 0, ALIGN_LEFT, "");
	reg_refresh_nout(&dyn.pad);

	reg_erase(&dyn.msg);
	switch (warn_type) {
	case WARN_PERF_DATA_FAIL:
		strncpy(content, "Perf event counting is failed!", WIN_LINECHAR_MAX);
		break;

	case WARN_INVALID_PID:
		strncpy(content, "Invalid process id, process exited.", WIN_LINECHAR_MAX);
		break;

	case WARN_INVALID_LWPID:
		strncpy(content, "Invalid lwp id, thread exited.", WIN_LINECHAR_MAX);
		break;

	case WARN_WAIT:
		strncpy(content, "Please wait ...", WIN_LINECHAR_MAX);
		break;

	case WARN_WAIT_PERF_LL_RESULT:
		strncpy(content, "Retrieving latency data ...", WIN_LINECHAR_MAX);
		break;

	case WARN_NOT_IMPL:
		strncpy(content, "Function is not implemented yet!", WIN_LINECHAR_MAX);
		break;

	case WARN_GO_HOME:
		strncpy(content, "Return to the Home window...", WIN_LINECHAR_MAX);
		break;

	case WARN_INVALID_NID:
		strncpy(content, "Invalid node id, node might be offlined.", WIN_LINECHAR_MAX);
		break;

	case WARN_INVALID_MAP:
		strncpy(content, "Cannot retrieve process address-space mapping.", WIN_LINECHAR_MAX);
		break;

	case WARN_INVALID_NUMAMAP:
		strncpy(content, "Cannot retrieve process memory NUMA mapping.", WIN_LINECHAR_MAX);
		break;

	case WARN_LL_NOT_SUPPORT:		
		strncpy(content, "PEBS (load latency) is unsupported or broken on the platform.",
			WIN_LINECHAR_MAX);
		break;

	default:
		content[0] = 0;
	}

	content[WIN_LINECHAR_MAX - 1] = 0;
	reg_line_write(&dyn.msg, 1, ALIGN_LEFT, content);
	reg_refresh_nout(&dyn.msg);
	reg_update_all();

	reg_win_destroy(&dyn.msg);
	reg_win_destroy(&dyn.pad);
}

/*
 * Each window has same fix regions: the title is at the top of window
 * and the note region is at the bottom of window.
 */
void
win_fix_init(void)
{
	reg_init(&s_note_reg, 0, g_scr_height - 1, g_scr_width, 1, A_REVERSE | A_BOLD);
	reg_init(&s_title_reg, 0, 0, g_scr_width, 1, 0);
	reg_update_all();
}

/*
 * Release the resources of fix regions in window.
 */
void
win_fix_fini(void)
{
	reg_win_destroy(&s_note_reg);
	reg_win_destroy(&s_title_reg);
}

/*
 * The common entry of window initialization
 */
int
win_dyn_init(void *p)
{
	page_t *page = (page_t *)p;
	dyn_win_t *win = &page->dyn_win;
	cmd_id_t cmd_id = CMD_ID(&page->cmd);
	int ret = -1;

	/*
	 * Initialization for the common regions for all windows.
	 */
	win->title = &s_title_reg;
	win->note = &s_note_reg;
	win->page = page;

	/*
	 * Initialization for the private regions according to
	 * different window type.
	 */
	switch (cmd_id) {
	case CMD_IR_NORMALIZE_ID:
		if ((win->dyn = topnproc_dyn_create(
		    WIN_TYPE_TOPNPROC)) == NULL) {
			goto L_EXIT;
		}

		win->type = WIN_TYPE_TOPNPROC;
		win->draw = topnproc_win_draw;
		win->scroll = topnproc_win_scroll;
		win->scroll_enter = topnproc_win_scrollenter;
		win->destroy = topnproc_win_destroy;
		break;

	case CMD_HOME_ID:		
		if ((win->dyn = topnproc_dyn_create(
		    WIN_TYPE_RAW_NUM)) == NULL) {
			goto L_EXIT;
		}

		win->type = WIN_TYPE_RAW_NUM;
		win->draw = topnproc_win_draw;
		win->scroll = topnproc_win_scroll;
		win->scroll_enter = topnproc_win_scrollenter;
		win->destroy = topnproc_win_destroy;
		break;

	case CMD_MONITOR_ID:
		if ((win->dyn = moni_dyn_create(
		    page, &win->draw, &win->type)) == NULL) {
			goto L_EXIT;
		}

		if (win->type == WIN_TYPE_MONILWP) {
			win->destroy = monilwp_win_destroy;
			win->scroll = monilwp_win_scroll;
		} else {
			win->destroy = moniproc_win_destroy;
			win->scroll = moniproc_win_scroll;
			win->scroll_enter = moniproc_win_scrollenter;
		}
		break;

	case CMD_LWP_ID:
		if ((win->dyn = topnlwp_dyn_create(page)) == NULL) {
			goto L_EXIT;
		}

		win->type = WIN_TYPE_TOPNLWP;
		win->draw = topnlwp_win_draw;
		win->scroll = topnlwp_win_scroll;
		win->scroll_enter = topnlwp_win_scrollenter;
		win->destroy = topnlwp_win_destroy;
		break;

	case CMD_NODE_OVERVIEW_ID:
		if ((win->dyn = nodeoverview_dyn_create()) == NULL) {
			goto L_EXIT;
		}

		win->type = WIN_TYPE_NODE_OVERVIEW;
		win->draw = nodeoverview_win_draw;
		win->scroll = nodeoverview_win_scroll;
		win->scroll_enter = nodeoverview_win_scrollenter;
		win->destroy = nodeoverview_win_destroy;
		break;

	case CMD_NODE_DETAIL_ID:
		if ((win->dyn = nodedetail_dyn_create(page)) == NULL) {
			goto L_EXIT;
		}

		win->type = WIN_TYPE_NODE_DETAIL;
		win->draw = nodedetail_win_draw;
		win->destroy = nodedetail_win_destroy;
		break;

	case CMD_CALLCHAIN_ID:
		if ((win->dyn = callchain_dyn_create(page)) == NULL) {
			goto L_EXIT;
		}

		win->type = WIN_TYPE_CALLCHAIN;
		win->draw = callchain_win_draw;
		win->destroy = callchain_win_destroy;
		win->scroll = callchain_win_scroll;
		break;

	case CMD_LAT_ID:
		if ((win->dyn = lat_dyn_create(page, &win->type)) == NULL) {
			goto L_EXIT;
		}

		win->draw = lat_win_draw;
		win->destroy = lat_win_destroy;
		win->scroll = lat_win_scroll;
		win->scroll_enter = lat_win_scrollenter;
		break;
		
	case CMD_LATNODE_ID:
		if ((win->dyn = latnode_dyn_create(page, &win->type)) == NULL) {
			goto L_EXIT;
		}

		win->draw = latnode_win_draw;
		win->destroy = latnode_win_destroy;
		win->scroll = latnode_win_scroll;
		break;
		
	case CMD_ACCDST_ID:
		if ((win->dyn = accdst_dyn_create(page, &win->type)) == NULL) {
			goto L_EXIT;
		}

		win->draw = accdst_win_draw;
		win->destroy = accdst_win_destroy;
		win->scroll = accdst_win_scroll;
		break;

	case CMD_LLCALLCHAIN_ID:
		if ((win->dyn = llcallchain_dyn_create(page)) == NULL) {
			goto L_EXIT;
		}

		win->type = WIN_TYPE_LLCALLCHAIN;
		win->draw = llcallchain_win_draw;
		win->destroy = llcallchain_win_destroy;
		win->scroll = llcallchain_win_scroll;
		break;

	default:
		goto L_EXIT;
	}
	
	win->inited = B_TRUE;
	ret = 0;

L_EXIT:
	return (ret);
}

/*
 * The common entry of window destroying
 */
void
win_dyn_fini(void *p)
{
	page_t *page = (page_t *)p;
	dyn_win_t *win = &page->dyn_win;

	if (win->inited && (win->destroy != NULL)) {
		win->destroy(win);
	}

	win->inited = B_FALSE;
}
