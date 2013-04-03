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

/* This file contains code to handle the 'command' of NumaTOP */

#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include "include/types.h"
#include "include/cmd.h"
#include "include/page.h"
#include "include/win.h"
#include "include/disp.h"

static switch_t s_switch[WIN_TYPE_NUM][CMD_NUM];

static int s_rawnum_sortkey[] = {
	SORT_KEY_RMA,
	SORT_KEY_LMA,
	SORT_KEY_RL,
	SORT_KEY_CPI,
	SORT_KEY_CPU
};

static int s_topnproc_sortkey[] = {
	SORT_KEY_RPI,
	SORT_KEY_LPI,
	SORT_KEY_RL,
	SORT_KEY_CPI,
	SORT_KEY_CPU
};

static int s_keeyrun_countid[] = {
	COUNT_RMA,
	COUNT_LMA,
	COUNT_CLK,
	COUNT_IR
};

static int
preop_switch2ll(cmd_t *cmd, boolean_t *smpl)
{
	*smpl = B_FALSE;
	if (!perf_ll_started()) {
		perf_allstop();
		if (perf_ll_start() != 0) {
			return (-1);
		}

		*smpl = B_TRUE;
	}

	return (0);	
}

static int
preop_switch2profiling(cmd_t *cmd, boolean_t *smpl)
{
	*smpl = B_FALSE;
	if (!perf_profiling_started()) {
		perf_allstop();
		if (perf_profiling_start() != 0) {
			return (-1);	
		}

		*smpl = B_TRUE;
	}

	return (0);
}

static int
preop_switch2callchain(cmd_t *cmd, boolean_t *smpl)
{
	page_t *cur = page_current_get();
	win_type_t type = PAGE_WIN_TYPE(cur);

	switch (type) {
	case WIN_TYPE_MONIPROC:
		CMD_CALLCHAIN(cmd)->pid = DYN_MONI_PROC(cur)->pid;
		CMD_CALLCHAIN(cmd)->lwpid = 0;
		break;
		
	case WIN_TYPE_MONILWP:
		CMD_CALLCHAIN(cmd)->pid = DYN_MONI_LWP(cur)->pid;
		CMD_CALLCHAIN(cmd)->lwpid = DYN_MONI_LWP(cur)->lwpid;
		break;

	default:
		return (-1);
	}	

	*smpl = B_TRUE;
	return (perf_profiling_partpause(COUNT_RMA));
}

static int
preop_leavecallchain(cmd_t *cmd, boolean_t *smpl)
{
	page_t *cur = page_current_get();
	count_id_t keeprun_id;
	
	if ((keeprun_id = DYN_CALLCHAIN(cur)->keeprun_id) != 0) {		
		perf_profiling_restore(keeprun_id);
	}

	*smpl = B_TRUE;
	return (0);
}

static int
preop_switch2accdst(cmd_t *cmd, boolean_t *smpl)
{
	page_t *cur = page_current_get();
	win_type_t type = PAGE_WIN_TYPE(cur);

	switch (type) {
	case WIN_TYPE_LAT_PROC:
		CMD_ACCDST(cmd)->pid = DYN_LAT(cur)->pid;
		CMD_ACCDST(cmd)->lwpid = 0;
		break;
		
	case WIN_TYPE_LAT_LWP:
		CMD_ACCDST(cmd)->pid = DYN_LAT(cur)->pid;
		CMD_ACCDST(cmd)->lwpid = DYN_LAT(cur)->lwpid;
		break;

	default:
		return (-1);
	}
	
	return (0);
}

static int
op_page_next(cmd_t *cmd, boolean_t smpl)
{
	/*
	 * Create a new page and append it after the current page in page
	 * list. The new page is showed in page_next_execute().
	 */
	if (page_create(cmd) != NULL) {
		if (page_next_execute(smpl)) {
			return (0);
		}
	}

	return (-1);
}

/* ARGSUSED */
static int
op_page_prev(cmd_t *cmd, boolean_t smpl)
{
	page_t *prev;
	
	if ((prev = page_curprev_get()) != NULL) {
		page_drop_next(prev);
		(void) page_current_set(prev);
		page_next_set(prev);
		if (!page_next_execute(smpl)) {
			return (-1);
		}
	}

	return (0);
}

/* ARGSUSED */
static int
op_refresh(cmd_t *cmd, boolean_t smpl)
{
	page_t *cur = page_current_get();

	page_next_set(cur);
	if (!page_smpl_start(cur)) {
		/*
		 * Refresh the current page by the latest sampling data.
		 */
		if (!page_next_execute(B_FALSE)) {
			return (-1);	
		}
	}	

	return (0);
}

static void
sortkey_set(int cmd_id, page_t *page)
{
	win_type_t win_type = PAGE_WIN_TYPE(page);
	int *arr = NULL;

	if (win_type == WIN_TYPE_RAW_NUM) {
		arr = s_rawnum_sortkey;
	} else {
		arr = s_topnproc_sortkey;
	}

	if ((cmd_id >= CMD_1_ID) && (cmd_id <= CMD_5_ID) && (arr != NULL)) {
		g_sortkey = arr[cmd_id - CMD_1_ID];
	}
}

static int
op_sort(cmd_t *cmd, boolean_t smpl)
{
	page_t *cur;
	int cmd_id;

	if ((cur = page_current_get()) != NULL) {
		cmd_id = CMD_ID(cmd);
		sortkey_set(cmd_id, cur);
		op_refresh(cmd, B_FALSE);
	}

	return (0);
}

/* ARGSUSED */
static int
op_home(cmd_t *cmd, boolean_t smpl)
{	
	page_list_fini();
	return (op_page_next(cmd, smpl));
}

static int
op_switch2ll(cmd_t *cmd, boolean_t smpl)
{
	page_t *cur = page_current_get();
	int type = PAGE_WIN_TYPE(cur);

	switch (type) {
	case WIN_TYPE_MONIPROC:
		CMD_LAT(cmd)->pid = DYN_MONI_PROC(cur)->pid;
		CMD_LAT(cmd)->lwpid = 0;
		break;
		
	case WIN_TYPE_MONILWP:
		CMD_LAT(cmd)->pid = DYN_MONI_LWP(cur)->pid;
		CMD_LAT(cmd)->lwpid = DYN_MONI_LWP(cur)->lwpid;
		break;

	default:
		return (-1);
	}

	return (op_page_next(cmd, smpl));
}

static count_id_t
keeprun_event_set(int cmd_id, page_t *page)
{
	dyn_callchain_t *dyn = (dyn_callchain_t *)(page->dyn_win.dyn);
	
	if ((cmd_id >= CMD_1_ID) && (cmd_id <= CMD_4_ID)) {
		dyn->keeprun_id = s_keeyrun_countid[cmd_id - CMD_1_ID];
	} else {
		dyn->keeprun_id = COUNT_INVALID;		
	}

	return (dyn->keeprun_id);
}

static int
op_callchain_event(cmd_t *cmd, boolean_t smpl)
{
	page_t *cur;
	count_id_t keeprun_id;
	int cmd_id;

	if ((cur = page_current_get()) != NULL) {
		cmd_id = CMD_ID(cmd);
		if ((keeprun_id = keeprun_event_set(cmd_id, cur)) == COUNT_INVALID) {
			return (0);	
		}

		perf_profiling_partpause(keeprun_id);
		op_refresh(cmd, smpl);
	}

	return (0);
}

static int
op_switch2llcallchain(cmd_t *cmd1, boolean_t smpl)
{
	cmd_llcallchain_t *cmd = (cmd_llcallchain_t *)cmd1;
	page_t *cur = page_current_get();
	dyn_lat_t *dyn;
	win_reg_t *data_reg;
	lat_line_t *lines;
	int i;

	dyn = (dyn_lat_t *)(cur->dyn_win.dyn);
	data_reg = &dyn->data;
	if ((lines = (lat_line_t *)(data_reg->buf)) == NULL) {
		return (-1);
	}
		
	if ((i = data_reg->scroll.highlight) == -1) {
		return (-1);
	}
		
	cmd->pid = dyn->pid;
	cmd->lwpid = dyn->lwpid;
	cmd->addr = lines[i].bufaddr.addr;
	cmd->size = lines[i].bufaddr.size;
	
	return (op_page_next(cmd1, smpl));
}

/*
 * Initialize for the "window switching" table.
 */
void
switch_table_init(void)
{
	int i;

	memset(s_switch, 0, sizeof (s_switch));
	for (i = 0; i < WIN_TYPE_NUM; i++) {
		s_switch[i][CMD_RESIZE_ID].op = op_refresh;
		s_switch[i][CMD_REFRESH_ID].op = op_refresh;
		s_switch[i][CMD_BACK_ID].op = op_page_prev;
		s_switch[i][CMD_HOME_ID].preop = preop_switch2profiling;
		s_switch[i][CMD_HOME_ID].op = op_home;
		s_switch[i][CMD_NODE_OVERVIEW_ID].preop = preop_switch2profiling;
		s_switch[i][CMD_NODE_OVERVIEW_ID].op = op_page_next;
	}

	/*
	 * Initialize for window type "WIN_TYPE_RAW_NUM"
	 */
	s_switch[WIN_TYPE_RAW_NUM][CMD_BACK_ID].op = NULL;
	s_switch[WIN_TYPE_RAW_NUM][CMD_MONITOR_ID].op = op_page_next;
	s_switch[WIN_TYPE_RAW_NUM][CMD_IR_NORMALIZE_ID].op = op_page_next;
	s_switch[WIN_TYPE_RAW_NUM][CMD_1_ID].op = op_sort;
	s_switch[WIN_TYPE_RAW_NUM][CMD_2_ID].op = op_sort;
	s_switch[WIN_TYPE_RAW_NUM][CMD_3_ID].op = op_sort;
	s_switch[WIN_TYPE_RAW_NUM][CMD_4_ID].op = op_sort;
	s_switch[WIN_TYPE_RAW_NUM][CMD_5_ID].op = op_sort;

	/*
	 * Initialize for window type "WIN_TYPE_TOPNPROC"
	 */
	s_switch[WIN_TYPE_TOPNPROC][CMD_MONITOR_ID].op = op_page_next;
	s_switch[WIN_TYPE_TOPNPROC][CMD_1_ID].op = op_sort;
	s_switch[WIN_TYPE_TOPNPROC][CMD_2_ID].op = op_sort;
	s_switch[WIN_TYPE_TOPNPROC][CMD_3_ID].op = op_sort;
	s_switch[WIN_TYPE_TOPNPROC][CMD_4_ID].op = op_sort;
	s_switch[WIN_TYPE_TOPNPROC][CMD_5_ID].op = op_sort;

	/*
	 * Initialize for window type "WIN_TYPE_MONIPROC"
	 */
	s_switch[WIN_TYPE_MONIPROC][CMD_LAT_ID].preop = preop_switch2ll;
	s_switch[WIN_TYPE_MONIPROC][CMD_LAT_ID].op = op_switch2ll;
	s_switch[WIN_TYPE_MONIPROC][CMD_LWP_ID].op = op_page_next;
	s_switch[WIN_TYPE_MONIPROC][CMD_CALLCHAIN_ID].preop = preop_switch2callchain;
	s_switch[WIN_TYPE_MONIPROC][CMD_CALLCHAIN_ID].op = op_page_next;

	/*
	 * Initialize for window type "WIN_TYPE_TOPNLWP"
	 */
	s_switch[WIN_TYPE_TOPNLWP][CMD_MONITOR_ID].op = op_page_next;

	/*
	 * Initialize for window type "WIN_TYPE_MONILWP"
	 */
	s_switch[WIN_TYPE_MONILWP][CMD_LAT_ID].preop = preop_switch2ll;
	s_switch[WIN_TYPE_MONILWP][CMD_LAT_ID].op = op_switch2ll;
	s_switch[WIN_TYPE_MONILWP][CMD_CALLCHAIN_ID].preop = preop_switch2callchain;
	s_switch[WIN_TYPE_MONILWP][CMD_CALLCHAIN_ID].op = op_page_next;

	/*
	 * Initialize for window type "WIN_TYPE_LAT_PROC"
	 */
	s_switch[WIN_TYPE_LAT_PROC][CMD_BACK_ID].preop = preop_switch2profiling;
	s_switch[WIN_TYPE_LAT_PROC][CMD_LLCALLCHAIN_ID].op = op_switch2llcallchain;
	s_switch[WIN_TYPE_LAT_PROC][CMD_LATNODE_ID].op = op_page_next;
	s_switch[WIN_TYPE_LAT_PROC][CMD_ACCDST_ID].preop = preop_switch2accdst;
	s_switch[WIN_TYPE_LAT_PROC][CMD_ACCDST_ID].op = op_page_next;
	s_switch[WIN_TYPE_LAT_PROC][CMD_NODE_OVERVIEW_ID].preop = NULL;
	s_switch[WIN_TYPE_LAT_PROC][CMD_NODE_OVERVIEW_ID].op = NULL;
	
	/*
	 * Initialize for window type "WIN_TYPE_LAT_LWP"
	 */	
	s_switch[WIN_TYPE_LAT_LWP][CMD_BACK_ID].preop = preop_switch2profiling;
	s_switch[WIN_TYPE_LAT_LWP][CMD_LLCALLCHAIN_ID].op = op_switch2llcallchain;
	s_switch[WIN_TYPE_LAT_LWP][CMD_LATNODE_ID].op = op_page_next;
	s_switch[WIN_TYPE_LAT_LWP][CMD_ACCDST_ID].preop = preop_switch2accdst;
	s_switch[WIN_TYPE_LAT_LWP][CMD_ACCDST_ID].op = op_page_next;
	s_switch[WIN_TYPE_LAT_LWP][CMD_NODE_OVERVIEW_ID].preop = NULL;
	s_switch[WIN_TYPE_LAT_LWP][CMD_NODE_OVERVIEW_ID].op = NULL;

	/*
	 * Initialize for window type "WIN_TYPE_LATNODE_PROC"
	 */	
	s_switch[WIN_TYPE_LATNODE_PROC][CMD_NODE_OVERVIEW_ID].preop = NULL;
	s_switch[WIN_TYPE_LATNODE_PROC][CMD_NODE_OVERVIEW_ID].op = NULL;

	/*
	 * Initialize for window type "WIN_TYPE_LATNODE_LWP"
	 */
	s_switch[WIN_TYPE_LATNODE_LWP][CMD_NODE_OVERVIEW_ID].preop = NULL;
	s_switch[WIN_TYPE_LATNODE_LWP][CMD_NODE_OVERVIEW_ID].op = NULL;

	/*
	 * Initialize for window type "WIN_TYPE_ACCDST_PROC"
	 */	
	s_switch[WIN_TYPE_ACCDST_PROC][CMD_NODE_OVERVIEW_ID].preop = NULL;
	s_switch[WIN_TYPE_ACCDST_PROC][CMD_NODE_OVERVIEW_ID].op = NULL;

	/*
	 * Initialize for window type "WIN_TYPE_ACCDST_LWP"
	 */	
	s_switch[WIN_TYPE_ACCDST_LWP][CMD_NODE_OVERVIEW_ID].preop = NULL;
	s_switch[WIN_TYPE_ACCDST_LWP][CMD_NODE_OVERVIEW_ID].op = NULL;

	/*
	 * Initialize for window type "WIN_TYPE_NODE_OVERVIEW"
	 */
	s_switch[WIN_TYPE_NODE_OVERVIEW][CMD_NODE_OVERVIEW_ID].preop = NULL;
	s_switch[WIN_TYPE_NODE_OVERVIEW][CMD_NODE_OVERVIEW_ID].op = NULL;
	s_switch[WIN_TYPE_NODE_OVERVIEW][CMD_BACK_ID].op = op_page_prev;
	s_switch[WIN_TYPE_NODE_OVERVIEW][CMD_NODE_DETAIL_ID].op = op_page_next;

	/*
	 * Initialize for window type "WIN_TYPE_CALLCHAIN"
	 */
	s_switch[WIN_TYPE_CALLCHAIN][CMD_BACK_ID].preop = preop_leavecallchain;
	s_switch[WIN_TYPE_CALLCHAIN][CMD_HOME_ID].preop = preop_leavecallchain;
	s_switch[WIN_TYPE_CALLCHAIN][CMD_NODE_OVERVIEW_ID].preop = NULL;
	s_switch[WIN_TYPE_CALLCHAIN][CMD_NODE_OVERVIEW_ID].op = NULL;
	s_switch[WIN_TYPE_CALLCHAIN][CMD_1_ID].op = op_callchain_event;
	s_switch[WIN_TYPE_CALLCHAIN][CMD_2_ID].op = op_callchain_event;
	s_switch[WIN_TYPE_CALLCHAIN][CMD_3_ID].op = op_callchain_event;
	s_switch[WIN_TYPE_CALLCHAIN][CMD_4_ID].op = op_callchain_event;

	/*
	 * Initialize for window type "WIN_TYPE_LLCALLCHAIN"
	 */
	s_switch[WIN_TYPE_LLCALLCHAIN][CMD_NODE_OVERVIEW_ID].preop = NULL;
	s_switch[WIN_TYPE_LLCALLCHAIN][CMD_NODE_OVERVIEW_ID].op = NULL;
}

static int
callchain_id_get(void)
{
	page_t *cur = page_current_get();

	if (cur == NULL) {
		return (CMD_INVALID_ID);
	}

	switch (PAGE_WIN_TYPE(cur)) {
	case WIN_TYPE_MONIPROC:
	case WIN_TYPE_MONILWP:
		return (CMD_CALLCHAIN_ID);

	case WIN_TYPE_LAT_PROC:
	case WIN_TYPE_LAT_LWP:
		return (CMD_LLCALLCHAIN_ID);

	default:
		return (CMD_INVALID_ID);
	}		
}

/*
 * Convert the character of hot-key to command id.
 */
int
cmd_id_get(char ch)
{
	switch (ch) {
	case CMD_HOME_CHAR:
		return (CMD_HOME_ID);

	case CMD_REFRESH_CHAR:
		return (CMD_REFRESH_ID);

	case CMD_QUIT_CHAR:
		return (CMD_QUIT_ID);

	case CMD_BACK_CHAR:
		return (CMD_BACK_ID);

	case CMD_LATENCY_CHAR:
		return (CMD_LAT_ID);

	case CMD_ACCDST_CHAR:
		return (CMD_ACCDST_ID);

	case CMD_IR_NORMALIZE_CHAR:
		return (CMD_IR_NORMALIZE_ID);

	case CMD_NODE_OVERVIEW_CHAR:
		return (CMD_NODE_OVERVIEW_ID);

	case CMD_CALLCHAIN_CHAR:
		return (callchain_id_get());

	case CMD_1_CHAR:
		return (CMD_1_ID);

	case CMD_2_CHAR:
		return (CMD_2_ID);

	case CMD_3_CHAR:
		return (CMD_3_ID);

	case CMD_4_CHAR:
		return (CMD_4_ID);

	case CMD_5_CHAR:
		return (CMD_5_ID);

	default:
		return (CMD_INVALID_ID);
	}
}

/*
 * The common entry to process all commands.
 */
void
cmd_execute(cmd_t *cmd, boolean_t *badcmd)
{
	cmd_id_t cmd_id;
	win_type_t type;
	page_t *cur;
	switch_t *s;
	boolean_t smpl = B_FALSE;
	
	if ((cmd_id = CMD_ID(cmd)) == CMD_INVALID_ID) {
		*badcmd = B_TRUE;
		return;
	} else {
		*badcmd = B_FALSE;
	}

	if ((cur = page_current_get()) == NULL) {
		/* It's the first window. */
		type = WIN_TYPE_RAW_NUM;			
	} else {
		type = PAGE_WIN_TYPE(cur);
	}

	s = &s_switch[type][cmd_id];
	if (s->preop != NULL) {
		(void) s->preop(cmd, &smpl);
	}

	if (s->op != NULL) {
		(void) s->op(cmd, smpl);
	}
}
