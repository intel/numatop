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
#include "include/os/os_page.h"
#include "include/os/os_cmd.h"
#include "include/os/plat.h"

int g_sortkey;

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

static switch_t s_switch[WIN_TYPE_NUM][CMD_NUM];

static int
preop_switch2profiling(cmd_t *cmd, boolean_t *smpl)
{
	return (os_preop_switch2profiling(cmd, smpl));
}

static int
preop_switch2ll(cmd_t *cmd, boolean_t *smpl)
{
	return (os_preop_switch2ll(cmd, smpl));
}

static int
preop_llrefresh(cmd_t *cmd, boolean_t *smpl)
{
	return (os_preop_llrefresh(cmd, smpl));
}

static int
preop_llmap_get(cmd_t *cmd, boolean_t *smpl)
{
	return (os_preop_llmap_get(cmd, smpl));
}

static int
preop_switch2ln(cmd_t *cmd, boolean_t *smpl)
{
	return (os_preop_switch2ln(cmd, smpl));
}

static int
preop_lnrefresh(cmd_t *cmd, boolean_t *smpl)
{
	return (os_preop_lnrefresh(cmd, smpl));
}

static int
preop_lnmap_get(cmd_t *cmd, boolean_t *smpl)
{
	return (os_preop_lnmap_get(cmd, smpl));
}

static int
preop_back2ll(cmd_t *cmd, boolean_t *smpl)
{
	return (os_preop_back2ll(cmd, smpl));
}

static int
preop_switch2callchain(cmd_t *cmd, boolean_t *smpl)
{
	return (os_preop_switch2callchain(cmd, smpl));
}

static int
preop_switch2accdst(cmd_t *cmd, boolean_t *smpl)
{
	return (os_preop_switch2accdst(cmd, smpl));
}

static int
preop_switch2pqoscmt(cmd_t *cmd, boolean_t *smpl)
{
	return (os_preop_switch2pqoscmt(cmd, smpl));
}

static int
preop_switch2pqosmbm(cmd_t *cmd, boolean_t *smpl)
{
	return (os_preop_switch2pqosmbm(cmd, smpl));
}

static int
preop_switch2uncore(cmd_t *cmd, boolean_t *smpl)
{
	return (os_preop_switch2uncore(cmd, smpl));
}

int
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

static int
preop_leavecallchain(cmd_t *cmd, boolean_t *smpl)
{
	return (os_preop_leavecallchain(cmd, smpl));
}

/* ARGSUSED */
static int
op_page_prev(cmd_t *cmd __attribute__((unused)),
	boolean_t smpl)
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
int
op_refresh(cmd_t *cmd __attribute__((unused)),
	boolean_t smpl __attribute__((unused)))
{
	page_t *cur = page_current_get();

	page_next_set(cur);
	if (!os_page_smpl_start(cur)) {
		/*
		 * Refresh the current page by the latest sampling data.
		 */
		if (!page_next_execute(B_FALSE)) {
			return (-1);
		}
	}

	return (0);
}

static int
op_llmap_stop(cmd_t *cmd, boolean_t smpl)
{
	return (os_op_llmap_stop(cmd, smpl));
}

static int
op_lnmap_stop(cmd_t *cmd, boolean_t smpl)
{
	return (os_op_lnmap_stop(cmd, smpl));
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

/* ARGSUSED */
static int
op_sort(cmd_t *cmd,
	boolean_t smpl __attribute__((unused)))
{
	page_t *cur;
	int cmd_id;

	if ((cur = page_current_get()) != NULL) {
		cmd_id = CMD_ID(cmd);
		sortkey_set(cmd_id, cur);
		(void) op_refresh(cmd, B_FALSE);
	}

	return (0);
}

static int
op_home(cmd_t *cmd, boolean_t smpl)
{
	page_list_fini();
	return (op_page_next(cmd, smpl));
}

static int
op_switch2ll(cmd_t *cmd, boolean_t smpl)
{
	return (os_op_switch2ll(cmd, smpl));
}

static int
op_callchain_count(cmd_t *cmd, boolean_t smpl)
{
	return (os_op_callchain_count(cmd, smpl));
}

static int
op_switch2llcallchain(cmd_t *cmd, boolean_t smpl)
{
	return (os_op_switch2llcallchain(cmd, smpl));	
}

/*
 * Initialize for the "window switching" table.
 */
void
switch_table_init(void)
{
	int i;

	(void) memset(s_switch, 0, sizeof (s_switch));
	for (i = 0; i < WIN_TYPE_NUM; i++) {
		s_switch[i][CMD_RESIZE_ID].op = op_refresh;
		s_switch[i][CMD_REFRESH_ID].op = op_refresh;
		s_switch[i][CMD_BACK_ID].op = op_page_prev;
		s_switch[i][CMD_HOME_ID].preop = preop_switch2profiling;
		s_switch[i][CMD_HOME_ID].op = op_home;
		s_switch[i][CMD_NODE_OVERVIEW_ID].preop =
		    preop_switch2profiling;
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
	s_switch[WIN_TYPE_MONIPROC][CMD_CALLCHAIN_ID].preop =
	    preop_switch2callchain;
	s_switch[WIN_TYPE_MONIPROC][CMD_CALLCHAIN_ID].op = op_page_next;
	s_switch[WIN_TYPE_MONIPROC][CMD_PQOS_CMT_ID].preop =
		preop_switch2pqoscmt;
	s_switch[WIN_TYPE_MONIPROC][CMD_PQOS_CMT_ID].op =
		op_page_next;

	/*
	 * Initialize for window type "WIN_TYPE_TOPNLWP"
	 */
	s_switch[WIN_TYPE_TOPNLWP][CMD_MONITOR_ID].op = op_page_next;

	/*
	 * Initialize for window type "WIN_TYPE_MONILWP"
	 */
	s_switch[WIN_TYPE_MONILWP][CMD_LAT_ID].preop = preop_switch2ll;
	s_switch[WIN_TYPE_MONILWP][CMD_LAT_ID].op = op_switch2ll;
	s_switch[WIN_TYPE_MONILWP][CMD_CALLCHAIN_ID].preop =
	    preop_switch2callchain;
	s_switch[WIN_TYPE_MONILWP][CMD_CALLCHAIN_ID].op = op_page_next;
	s_switch[WIN_TYPE_MONILWP][CMD_PQOS_CMT_ID].preop =
		preop_switch2pqoscmt;
	s_switch[WIN_TYPE_MONILWP][CMD_PQOS_CMT_ID].op =
		op_page_next;

	/*
	 * Initialize for window type "WIN_TYPE_LAT_PROC"
	 */	
	s_switch[WIN_TYPE_LAT_PROC][CMD_REFRESH_ID].preop = preop_llrefresh;
	s_switch[WIN_TYPE_LAT_PROC][CMD_BACK_ID].preop = preop_switch2profiling;
	s_switch[WIN_TYPE_LAT_PROC][CMD_LLCALLCHAIN_ID].op =
		op_switch2llcallchain;
	s_switch[WIN_TYPE_LAT_PROC][CMD_LATNODE_ID].preop = preop_switch2ln;
	s_switch[WIN_TYPE_LAT_PROC][CMD_LATNODE_ID].op = op_page_next;
	s_switch[WIN_TYPE_LAT_PROC][CMD_ACCDST_ID].preop = preop_switch2accdst;
	s_switch[WIN_TYPE_LAT_PROC][CMD_ACCDST_ID].op = op_page_next;
	s_switch[WIN_TYPE_LAT_PROC][CMD_MAP_GET_ID].preop = preop_llmap_get;
	s_switch[WIN_TYPE_LAT_PROC][CMD_MAP_GET_ID].op = op_refresh;
	s_switch[WIN_TYPE_LAT_PROC][CMD_MAP_STOP_ID].op = op_llmap_stop;
	s_switch[WIN_TYPE_LAT_PROC][CMD_NODE_OVERVIEW_ID].preop = NULL;
	s_switch[WIN_TYPE_LAT_PROC][CMD_NODE_OVERVIEW_ID].op = NULL;

	/*
	 * Initialize for window type "WIN_TYPE_LAT_LWP"
	 */
	s_switch[WIN_TYPE_LAT_LWP][CMD_REFRESH_ID].preop = preop_llrefresh;
	s_switch[WIN_TYPE_LAT_LWP][CMD_BACK_ID].preop = preop_switch2profiling;
	s_switch[WIN_TYPE_LAT_LWP][CMD_LLCALLCHAIN_ID].op =
		op_switch2llcallchain;
	s_switch[WIN_TYPE_LAT_LWP][CMD_LATNODE_ID].preop = preop_switch2ln;
	s_switch[WIN_TYPE_LAT_LWP][CMD_LATNODE_ID].op = op_page_next;
	s_switch[WIN_TYPE_LAT_LWP][CMD_ACCDST_ID].preop = preop_switch2accdst;
	s_switch[WIN_TYPE_LAT_LWP][CMD_ACCDST_ID].op = op_page_next;
	s_switch[WIN_TYPE_LAT_LWP][CMD_MAP_GET_ID].preop = preop_llmap_get;
	s_switch[WIN_TYPE_LAT_LWP][CMD_MAP_GET_ID].op = op_refresh;
	s_switch[WIN_TYPE_LAT_LWP][CMD_MAP_STOP_ID].op = op_llmap_stop;
	s_switch[WIN_TYPE_LAT_LWP][CMD_NODE_OVERVIEW_ID].preop = NULL;
	s_switch[WIN_TYPE_LAT_LWP][CMD_NODE_OVERVIEW_ID].op = NULL;

	/*
	 * Initialize for window type "WIN_TYPE_LATNODE_PROC"
	 */
	s_switch[WIN_TYPE_LATNODE_PROC][CMD_REFRESH_ID].preop = preop_lnrefresh;
	s_switch[WIN_TYPE_LATNODE_PROC][CMD_BACK_ID].preop = preop_back2ll;
	s_switch[WIN_TYPE_LATNODE_PROC][CMD_MAP_GET_ID].preop = preop_lnmap_get;
	s_switch[WIN_TYPE_LATNODE_PROC][CMD_MAP_GET_ID].op = op_refresh;
	s_switch[WIN_TYPE_LATNODE_PROC][CMD_MAP_STOP_ID].op = op_lnmap_stop;
	s_switch[WIN_TYPE_LATNODE_PROC][CMD_NODE_OVERVIEW_ID].preop = NULL;
	s_switch[WIN_TYPE_LATNODE_PROC][CMD_NODE_OVERVIEW_ID].op = NULL;

	/*
	 * Initialize for window type "WIN_TYPE_LATNODE_LWP"
	 */
	s_switch[WIN_TYPE_LATNODE_LWP][CMD_REFRESH_ID].preop = preop_lnrefresh;
	s_switch[WIN_TYPE_LATNODE_LWP][CMD_BACK_ID].preop = preop_back2ll;
	s_switch[WIN_TYPE_LATNODE_LWP][CMD_MAP_GET_ID].preop = preop_lnmap_get;
	s_switch[WIN_TYPE_LATNODE_LWP][CMD_MAP_GET_ID].op = op_refresh;
	s_switch[WIN_TYPE_LATNODE_LWP][CMD_MAP_STOP_ID].op = op_lnmap_stop;
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
	s_switch[WIN_TYPE_NODE_OVERVIEW][CMD_NODE_DETAIL_ID].preop =
		preop_switch2uncore;	
	s_switch[WIN_TYPE_NODE_OVERVIEW][CMD_NODE_DETAIL_ID].op = op_page_next;

	/*
	 * Initialize for window type "WIN_TYPE_NODE_DETAIL"
	 */
	s_switch[WIN_TYPE_NODE_DETAIL][CMD_BACK_ID].preop =
		preop_switch2profiling;

	/*
	 * Initialize for window type "WIN_TYPE_CALLCHAIN"
	 */
	s_switch[WIN_TYPE_CALLCHAIN][CMD_BACK_ID].preop =
	    preop_leavecallchain;;
	s_switch[WIN_TYPE_CALLCHAIN][CMD_HOME_ID].preop =
	    preop_leavecallchain;;
	s_switch[WIN_TYPE_CALLCHAIN][CMD_NODE_OVERVIEW_ID].preop = NULL;
	s_switch[WIN_TYPE_CALLCHAIN][CMD_NODE_OVERVIEW_ID].op = NULL;
	s_switch[WIN_TYPE_CALLCHAIN][CMD_1_ID].op = op_callchain_count;
	s_switch[WIN_TYPE_CALLCHAIN][CMD_2_ID].op = op_callchain_count;
	s_switch[WIN_TYPE_CALLCHAIN][CMD_3_ID].op = op_callchain_count;
	s_switch[WIN_TYPE_CALLCHAIN][CMD_4_ID].op = op_callchain_count;

	/*	 
	 * Initialize for window type "WIN_TYPE_LLCALLCHAIN"	 
	 */
	s_switch[WIN_TYPE_LLCALLCHAIN][CMD_NODE_OVERVIEW_ID].preop = NULL;
	s_switch[WIN_TYPE_LLCALLCHAIN][CMD_NODE_OVERVIEW_ID].op = NULL;

	/*
	 * Initialize for window type "WIN_TYPE_PQOS_CMT_TOPNPROC"
	 */
	s_switch[WIN_TYPE_PQOS_CMT_TOPNPROC][CMD_BACK_ID].preop =
		preop_switch2profiling;

	/*
	 * Initialize for window type "WIN_TYPE_PQOS_CMT_MONIPROC"
	 */
	s_switch[WIN_TYPE_PQOS_CMT_MONIPROC][CMD_BACK_ID].preop =
		preop_switch2profiling;
	s_switch[WIN_TYPE_PQOS_CMT_MONIPROC][CMD_PQOS_MBM_ID].preop =
		preop_switch2pqosmbm;
	s_switch[WIN_TYPE_PQOS_CMT_MONIPROC][CMD_PQOS_MBM_ID].op =
		op_page_next;

	/*
	 * Initialize for window type "WIN_TYPE_PQOS_CMT_MONILWP"
	 */
	s_switch[WIN_TYPE_PQOS_CMT_MONILWP][CMD_BACK_ID].preop =
		preop_switch2profiling;
	s_switch[WIN_TYPE_PQOS_CMT_MONILWP][CMD_PQOS_MBM_ID].preop =
		preop_switch2pqosmbm;
	s_switch[WIN_TYPE_PQOS_CMT_MONILWP][CMD_PQOS_MBM_ID].op =
		op_page_next;

	/*
	 * Initialize for window type "WIN_TYPE_PQOS_MBM_MONIPROC"
	 */
	s_switch[WIN_TYPE_PQOS_MBM_MONIPROC][CMD_BACK_ID].preop =
		preop_switch2pqoscmt;

	/*
	 * Initialize for window type "WIN_TYPE_PQOS_MBM_MONILWP"
	 */
	s_switch[WIN_TYPE_PQOS_MBM_MONILWP][CMD_BACK_ID].preop =
		preop_switch2pqoscmt;
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

	case CMD_MAP_GET_CHAR:
		return (CMD_MAP_GET_ID);

	case CMD_MAP_STOP_CHAR:
		return (CMD_MAP_STOP_ID);

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

	case CMD_PQOS_CMT_CHAR:
		if (g_cmt_enabled)
			return (CMD_PQOS_CMT_ID);

		return (CMD_INVALID_ID);

	case CMD_PQOS_MBM_CHAR:
		if (g_cmt_enabled)
			return (CMD_PQOS_MBM_ID);

		return (CMD_INVALID_ID);

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
	boolean_t b = B_TRUE, smpl = B_FALSE;

	if ((cmd_id = CMD_ID(cmd)) == CMD_INVALID_ID) {
		goto L_EXIT;
	}

	b = B_FALSE;

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

L_EXIT:
	if (badcmd != NULL) {
		*badcmd = b;
	}
}
