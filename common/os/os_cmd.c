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
#include <ctype.h>
#include <strings.h>
#include "../include/types.h"
#include "../include/page.h"
#include "../include/win.h"
#include "../include/perf.h"
#include "../include/cmd.h"
#include "../include/disp.h"
#include "../include/os/map.h"
#include "../include/os/os_cmd.h"

static int s_callchain_ui_countid[] = {
	UI_COUNT_RMA,
	UI_COUNT_LMA,
	UI_COUNT_CLK,
	UI_COUNT_IR
};

int
os_preop_switch2profiling(cmd_t *cmd __attribute__((unused)), boolean_t *smpl)
{
	*smpl = B_FALSE;

	if (perf_pqos_cmt_started()) {
		perf_pqos_cmt_stop(0, 0);
		*smpl = B_TRUE;
	}

	if (perf_uncore_started()) {
		perf_uncore_stop(-1);
		*smpl = B_TRUE;
	}

	if (!perf_profiling_started()) {
		perf_allstop();
		if (perf_profiling_start() != 0) {
			return (-1);	
		}

		*smpl = B_TRUE;
	}

	return (0);
}

int
os_preop_switch2ll(cmd_t *cmd __attribute__((unused)), boolean_t *smpl)
{
	*smpl = B_FALSE;
	if (!perf_ll_started()) {
		perf_allstop();
		if (perf_ll_start(0) != 0) {
			return (-1);
		}

		*smpl = B_TRUE;
	}

	return (0);
}

int
os_preop_llrefresh(cmd_t *cmd __attribute__((unused)),
	boolean_t *smpl __attribute__((unused)))
{
	/* Not supported on Linux. */
	return (0);
}

int
os_preop_llmap_get(cmd_t *cmd __attribute__((unused)),
	boolean_t *smpl __attribute__((unused)))
{
	/* Not supported on Linux. */
	return (0);
}

int
os_preop_switch2ln(cmd_t *cmd __attribute__((unused)),
	boolean_t *smpl __attribute__((unused)))
{
	/* Not supported on Linux. */
	return (0);
}

int
os_preop_lnrefresh(cmd_t *cmd __attribute__((unused)),
	boolean_t *smpl __attribute__((unused)))
{
	/* Not supported on Linux. */
	return (0);
}

int
os_preop_lnmap_get(cmd_t *cmd __attribute__((unused)),
	boolean_t *smpl __attribute__((unused)))
{
	/* Not supported on Linux. */
	return (0);
}

int
os_preop_back2ll(cmd_t *cmd __attribute__((unused)),
	boolean_t *smpl __attribute__((unused)))
{
	return (0);
}

int
os_preop_switch2callchain(cmd_t *cmd, boolean_t *smpl)
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
	return (perf_profiling_partpause(UI_COUNT_RMA));
}

int
os_preop_switch2accdst(cmd_t *cmd,
	boolean_t *smpl __attribute__((unused)))
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

int
os_preop_leavecallchain(cmd_t *cmd __attribute__((unused)), boolean_t *smpl)
{
	page_t *cur = page_current_get();
	ui_count_id_t ui_countid;

	if ((ui_countid = DYN_CALLCHAIN(cur)->ui_countid) != 0) {
		perf_profiling_restore(ui_countid);
	}

	*smpl = B_TRUE;
	return (0);
}

int
os_preop_switch2pqoscmt(cmd_t *cmd,
	boolean_t *smpl __attribute__((unused)))
{
	page_t *cur = page_current_get();
	win_type_t type = PAGE_WIN_TYPE(cur);
	int ret = 0;

	switch (type) {
	case WIN_TYPE_MONIPROC:
		CMD_PQOS_CMT(cmd)->pid = DYN_MONI_PROC(cur)->pid;
		CMD_PQOS_CMT(cmd)->lwpid = 0;
		CMD_PQOS_CMT(cmd)->flags = PERF_PQOS_FLAG_LLC;
		break;

	case WIN_TYPE_RAW_NUM:
	case WIN_TYPE_TOPNPROC:
		CMD_PQOS_CMT(cmd)->pid = 0;
		CMD_PQOS_CMT(cmd)->lwpid = 0;
		CMD_PQOS_CMT(cmd)->flags = PERF_PQOS_FLAG_LLC;
		break;

	case WIN_TYPE_MONILWP:
		CMD_PQOS_CMT(cmd)->pid = DYN_MONI_LWP(cur)->pid;
		CMD_PQOS_CMT(cmd)->lwpid = DYN_MONI_LWP(cur)->lwpid;
		CMD_PQOS_CMT(cmd)->flags = PERF_PQOS_FLAG_LLC;
		break;

	case WIN_TYPE_PQOS_MBM_MONIPROC:
	case WIN_TYPE_PQOS_MBM_MONILWP:
		CMD_PQOS_CMT(cmd)->pid = DYN_PQOS_MBM_PROC(cur)->pid;
		CMD_PQOS_CMT(cmd)->lwpid = DYN_PQOS_MBM_PROC(cur)->lwpid;
		CMD_PQOS_CMT(cmd)->flags = PERF_PQOS_FLAG_LLC;
		break;

	default:
		return (-1);
	}

	perf_pqos_cmt_stop(CMD_PQOS_CMT(cmd)->pid, CMD_PQOS_CMT(cmd)->lwpid);

	if (perf_profiling_smpl(B_FALSE) != 0)
		return -1;

	if (disp_flag2_wait() != DISP_FLAG_PROFILING_DATA_READY)
		return -1;

	if (CMD_PQOS_CMT(cmd)->pid == 0) {
		ret = perf_pqos_active_proc_setup(CMD_PQOS_CMT(cmd)->flags, B_FALSE);
	} else {
		ret = perf_pqos_proc_setup(CMD_PQOS_CMT(cmd)->pid,
			CMD_PQOS_CMT(cmd)->lwpid, CMD_PQOS_CMT(cmd)->flags);
	}

	return (ret);
}

int
os_preop_switch2pqosmbm(cmd_t *cmd,
	boolean_t *smpl __attribute__((unused)))
{
	page_t *cur = page_current_get();
	win_type_t type = PAGE_WIN_TYPE(cur);
	int ret = 0;

	if ((type == WIN_TYPE_PQOS_CMT_MONIPROC) || (type == WIN_TYPE_PQOS_CMT_MONILWP)) {

		if (perf_profiling_smpl(B_FALSE) != 0)
			return -1;

		if (disp_flag2_wait() != DISP_FLAG_PROFILING_DATA_READY)
			return -1;

		CMD_PQOS_MBM(cmd)->pid = DYN_PQOS_CMT_PROC(cur)->pid;
		CMD_PQOS_MBM(cmd)->lwpid = DYN_PQOS_CMT_PROC(cur)->lwpid;
		CMD_PQOS_MBM(cmd)->flags = PERF_PQOS_FLAG_TOTAL_BW | PERF_PQOS_FLAG_LOCAL_BW;

		perf_pqos_cmt_stop(CMD_PQOS_MBM(cmd)->pid, CMD_PQOS_MBM(cmd)->lwpid);

		ret = perf_pqos_proc_setup(CMD_PQOS_MBM(cmd)->pid,
			CMD_PQOS_MBM(cmd)->lwpid, CMD_PQOS_MBM(cmd)->flags);
	}

	return (ret);
}

int
os_preop_switch2uncore(cmd_t *cmd,
	boolean_t *smpl __attribute__((unused)))
{
	page_t *cur = page_current_get();
	win_type_t type = PAGE_WIN_TYPE(cur);
	int ret = 0;

	if (type == WIN_TYPE_NODE_OVERVIEW) {

		if (perf_profiling_smpl(B_FALSE) != 0)
			return -1;

		if (disp_flag2_wait() != DISP_FLAG_PROFILING_DATA_READY)
			return -1;

		perf_uncore_stop(-1);
		ret = perf_uncore_setup(CMD_NODE_DETAIL(cmd)->nid);
	}

	return (ret);
}

int
os_op_llmap_stop(cmd_t *cmd __attribute__((unused)),
	boolean_t smpl __attribute__((unused)))
{
	/* Not supported on Linux. */
	return (0);
}

int
os_op_lnmap_stop(cmd_t *cmd __attribute__((unused)),
	boolean_t smpl __attribute__((unused)))
{
	/* Not supported on Linux. */
	return (0);
}

int
os_op_switch2ll(cmd_t *cmd, boolean_t smpl)
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

static ui_count_id_t
callchain_countid_set(int cmd_id, page_t *page)
{
	dyn_callchain_t *dyn = (dyn_callchain_t *)(page->dyn_win.dyn);

	if ((cmd_id >= CMD_1_ID) && (cmd_id <= CMD_4_ID)) {
		dyn->ui_countid = s_callchain_ui_countid[cmd_id - CMD_1_ID];
	} else {
		dyn->ui_countid = UI_COUNT_INVALID;
	}

	return (dyn->ui_countid);
}

int
os_op_callchain_count(cmd_t *cmd, boolean_t smpl)
{
	page_t *cur;
	ui_count_id_t ui_countid;
	int cmd_id;

	if ((cur = page_current_get()) != NULL) {
		cmd_id = CMD_ID(cmd);
		ui_countid = callchain_countid_set(cmd_id, cur);
		if (ui_countid == UI_COUNT_INVALID) {
			return (0);	
		}

		perf_profiling_partpause(ui_countid);
		op_refresh(cmd, smpl);
	}

	return (0);
}

int
os_op_switch2llcallchain(cmd_t *cmd1, boolean_t smpl)
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
