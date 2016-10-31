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

#ifndef _NUMATOP_WIN_H
#define	_NUMATOP_WIN_H

#include <sys/types.h>
#include <sys/time.h>
#include <inttypes.h>
#include <pthread.h>
#include "types.h"
#include "reg.h"
#include "./os/os_win.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	NUMATOP_TITLE	"NumaTOP v2.0, (C) 2015 Intel Corporation"
#define	CMD_CAPTION		"Command: "
#define	WIN_PROCNAME_SIZE	12
#define	WIN_DESCBUF_SIZE	32
#define	WIN_CALLCHAIN_NUM	5
#define	WIN_LINECHAR_MAX	1024
#define	WIN_NLINES_MAX		4096

#define	GO_HOME_WAIT	3

#define	NOTE_DEFAULT \
	"Q: Quit; H: Home; B: Back; R: Refresh; N: Node"

#define	NOTE_DEFAULT_LLC \
	"Q: Quit; H: Home; B: Back; R: Refresh; N: Node; O: LLC OCCUPANCY"

#define	NOTE_TOPNPROC_RAW \
	"Q: Quit; H: Home; R: Refresh; I: IR Normalize; N: Node"

#define	NOTE_TOPNPROC_RAW_LLC \
	"Q: Quit; H: Home; R: Refresh; I: IR Normalize; N: Node; O: LLC OCCUPANCY"

#define NOTE_TOPNPROC	NOTE_DEFAULT
#define	NOTE_TOPNPROC_LLC	NOTE_DEFAULT_LLC

#define	NOTE_TOPNLWP	NOTE_DEFAULT

#define	NOTE_MONIPROC \
	"Q: Quit; H: Home; B: Back; R: Refresh; " \
	"N: Node; L: Latency; C: Call-Chain"

#define	NOTE_MONIPROC_LLC \
	"Q;  H;  B;  R;  " \
	"N: Node; L: Latency; C: Call-Chain; O: LLC OCCUPANCY"

#define	NOTE_MONILWP 	NOTE_MONIPROC

#define	NOTE_MONILWP_LLC	NOTE_MONIPROC_LLC

#define	NOTE_NONODE \
	"Q: Quit; H: Home; B: Back; R: Refresh"

#define	NOTE_ACCDST	NOTE_NONODE
#define	NOTE_NODEOVERVIEW NOTE_NONODE
#define	NOTE_NODEDETAIL NOTE_NONODE
#define	NOTE_CALLCHAIN	NOTE_NONODE
#define NOTE_PQOS_CMT_TOPNPROC	NOTE_NONODE
#define NOTE_PQOS_MBM	NOTE_NONODE

#define NOTE_PQOS_CMT_MONI	\
	"Q: Quit; H: Home; B: Back; R: Refresh; P: Memory Bandwidth"

#define	NOTE_INVALID_PID \
	"Invalid process id! (Q: Quit; H: Home)"

#define	NOTE_INVALID_LWPID \
	"Invalid lwp id! (Q: Quit; H: Home)"

#define	NOTE_INVALID_MAP \
	"No memory mapping found! (Q: Quit; H: Home; B: Back)"

#define	NOTE_INVALID_NUMAMAP \
	"No memory NUMA mapping found! (Q: Quit; H: Home; B: Back)"

#define	CAPTION_PID			"PID"
#define	CAPTION_LWP			"LWP"
#define	CAPTION_RPI			"RPI(K)"
#define	CAPTION_LPI			"LPI(K)"
#define	CAPTION_CPI			"CPI"
#define	CAPTION_CPU			"CPU%%"
#define	CAPTION_NID			"NODE"
#define	CAPTION_PROC		"PROC"
#define	CAPTION_ADDR		"ADDR"
#define	CAPTION_SIZE		"SIZE"
#define	CAPTION_RMA			"RMA(K)"
#define	CAPTION_LMA			"LMA(K)"
#define	CAPTION_RL			"RMA/LMA"
#define	CAPTION_DESC		"DESC"
#define	CAPTION_BUFHIT		"ACCESS%%"
#define	CAPTION_AVGLAT		"LAT(ns)"
#define	CAPTION_MEM_ALL		"MEM.ALL"
#define	CAPTION_MEM_FREE	"MEM.FREE"
#define CAPTION_LLC_OCCUPANCY	"LLC.OCCUPANCY(MB)"
#define CAPTION_TOTAL_BW	"MBAND.TOTAL"
#define CAPTION_LOCAL_BW	"MBAND.LOCAL"

typedef enum {
	WIN_TYPE_RAW_NUM = 0,
	WIN_TYPE_TOPNPROC,
	WIN_TYPE_TOPNLWP,
	WIN_TYPE_MONILWP,
	WIN_TYPE_MONIPROC,
	WIN_TYPE_LAT_PROC,
	WIN_TYPE_LAT_LWP,
	WIN_TYPE_LATNODE_PROC,
	WIN_TYPE_LATNODE_LWP,
	WIN_TYPE_NODE_OVERVIEW,
	WIN_TYPE_NODE_DETAIL,
	WIN_TYPE_CALLCHAIN,
	WIN_TYPE_LLCALLCHAIN,
	WIN_TYPE_ACCDST_PROC,
	WIN_TYPE_ACCDST_LWP,
	WIN_TYPE_PQOS_CMT_TOPNPROC,
	WIN_TYPE_PQOS_CMT_MONIPROC,
	WIN_TYPE_PQOS_CMT_MONILWP,
	WIN_TYPE_PQOS_MBM_MONIPROC,
	WIN_TYPE_PQOS_MBM_MONILWP,
} win_type_t;

#define	WIN_TYPE_NUM		20

typedef enum {
	WARN_INVALID = 0,
	WARN_PERF_DATA_FAIL,
	WARN_INVALID_PID,
	WARN_INVALID_LWPID,
	WARN_WAIT,
	WARN_WAIT_PERF_LL_RESULT,
	WARN_NOT_IMPL,
	WARN_GO_HOME,
	WARN_INVALID_NID,
	WARN_INVALID_MAP,
	WARN_INVALID_NUMAMAP,
	WARN_LL_NOT_SUPPORT,
	WARN_STOP
} warn_type_t;

typedef struct _dyn_win {
	win_type_t type;
	boolean_t inited;
	win_reg_t *title;
	void *dyn;
	win_reg_t *note;
	void *page;
	boolean_t (*draw)(struct _dyn_win *);
	void (*scroll)(struct _dyn_win *, int);
	void (*scroll_enter)(struct _dyn_win *);
	void (*destroy)(struct _dyn_win *);
} dyn_win_t;

typedef struct _win_countvalue {
	double rpi;
	double lpi;
	double cpu;
	double cpi;
	double rma;
	double lma;
	double rl;
} win_countvalue_t;

typedef struct _dyn_topnproc {
	win_reg_t summary;
	win_reg_t caption;
	win_reg_t data;
	win_reg_t hint;
} dyn_topnproc_t;

typedef struct _topnproc_line {
	win_countvalue_t value;
	char proc_name[WIN_PROCNAME_SIZE];
	int pid;
	int nlwp;
} topnproc_line_t;

typedef struct _dyn_moniproc {
	pid_t pid;
	win_reg_t msg;
	win_reg_t caption_cur;
	win_reg_t data_cur;
	win_reg_t hint;
} dyn_moniproc_t;

typedef struct _dyn_monilwp {
	pid_t pid;
	int lwpid;
	win_reg_t msg;
	win_reg_t caption_cur;
	win_reg_t data_cur;
	win_reg_t hint;
} dyn_monilwp_t;

typedef struct _moni_line {
	win_countvalue_t value;
	int nid;
	pid_t pid;
} moni_line_t;

typedef struct _dyn_topnlwp {
	pid_t pid;
	win_reg_t msg;
	win_reg_t caption;
	win_reg_t data;
	win_reg_t hint;
} dyn_topnlwp_t;

typedef struct _topnlwp_line {
	win_countvalue_t value;
	pid_t pid;
	int lwpid;
} topnlwp_line_t;

typedef struct _dyn_lat {
	pid_t pid;
	int lwpid;
	win_reg_t msg;
	win_reg_t caption;
	win_reg_t data;
} dyn_lat_t;

typedef struct _dyn_latnode {
	pid_t pid;
	int lwpid;
	uint64_t addr;
	uint64_t size;
	win_reg_t msg;
	win_reg_t note;
	win_reg_t caption;
	win_reg_t data;
} dyn_latnode_t;

typedef struct _lat_line {
	bufaddr_t bufaddr;	/* must be the first field */
	int naccess;
	int latency;
	int nsamples;
	pid_t pid;
	int lwpid;
	int nid;
	boolean_t nid_show;
	char desc[WIN_DESCBUF_SIZE];
} lat_line_t;

typedef struct _dyn_accdst {
	pid_t pid;
	int lwpid;
	win_reg_t msg;
	win_reg_t caption;
	win_reg_t data;
	win_reg_t hint;
} dyn_accdst_t;

typedef struct _accdst_line {
	int nid;
	double access_ratio;
	int latency;
} accdst_line_t;

typedef struct _dyn_nodeoverview {
	win_reg_t msg;
	win_reg_t caption_cur;
	win_reg_t data_cur;
	win_reg_t hint;
} dyn_nodeoverview_t;

typedef struct _nodeoverview_line {
	win_countvalue_t value;
	double mem_all;
	double mem_free;
	int nid;
} nodeoverview_line_t;

typedef struct _dyn_nodedetail {
	int nid;
	win_reg_t msg;
	win_reg_t node_data;
	win_reg_t hint;
} dyn_nodedetail_t;

typedef struct _dyn_callchain {
	pid_t pid;
	int lwpid;
	count_id_t countid;
	win_reg_t msg;
	win_reg_t caption;
	win_reg_t pad;
	win_reg_t data;
	win_reg_t hint;
} dyn_callchain_t;

typedef struct _callchain_line {
	char content[WIN_LINECHAR_MAX];
} callchain_line_t;

typedef struct _dyn_llcallchain {
	pid_t pid;
	int lwpid;
	uint64_t addr;
	uint64_t size;
	char desc[WIN_DESCBUF_SIZE];
	win_reg_t msg;
	win_reg_t buf_caption;
	win_reg_t buf_data;
	win_reg_t chain_caption;
	win_reg_t pad;
	win_reg_t chain_data;
} dyn_llcallchain_t;

typedef struct _dyn_pqos_cmt_proc {
	pid_t pid;
	int lwpid;
	win_reg_t summary;
	win_reg_t caption;
	win_reg_t data;
	win_reg_t hint;
} dyn_pqos_cmt_proc_t;

typedef struct _pqos_cmt_proc_line {
	win_countvalue_t value;
	uint64_t llc_occupancy;
	int pid;
	int lwpid;
	int nlwp;
	int fd;
	char proc_name[WIN_PROCNAME_SIZE];
} pqos_cmt_proc_line_t;

typedef struct _dyn_pqos_mbm_proc {
	pid_t pid;
	int lwpid;
	win_reg_t summary;
	win_reg_t caption;
	win_reg_t data;
	win_reg_t hint;
} dyn_pqos_mbm_proc_t;

typedef struct _pqos_mbm_proc_line {
	win_countvalue_t value;
	uint64_t totalbw_scaled;
	uint64_t localbw_scaled;
	int pid;
	int lwpid;
	int nlwp;
	int totalbw_fd;
	int localbw_fd;
	char proc_name[WIN_PROCNAME_SIZE];
} pqos_mbm_proc_line_t;

typedef struct _dyn_warn {
	win_reg_t msg;
	win_reg_t pad;
} dyn_warn_t;

#define	DYN_MONI_PROC(page) \
	((dyn_moniproc_t *)((page)->dyn_win.dyn))

#define	DYN_MONI_LWP(page) \
	((dyn_monilwp_t *)((page)->dyn_win.dyn))

#define	DYN_LAT(page) \
	((dyn_lat_t *)((page)->dyn_win.dyn))

#define	DYN_LATNODE(page) \
	((dyn_latnode_t *)((page)->dyn_win.dyn))

#define	DYN_ACCDST(page) \
	((dyn_accdst_t *)((page)->dyn_win.dyn))

#define	DYN_NODEOVERVIEW(page) \
	((dyn_nodeoverview_t *)((page)->dyn_win.dyn))

#define	DYN_NODEDETAIL(page) \
	((dyn_nodedetail_t *)((page)->dyn_win.dyn))

#define	DYN_CALLCHAIN(page) \
	((dyn_callchain_t *)((page)->dyn_win.dyn))

#define	DYN_LLCALLCHAIN(page) \
	((dyn_llcallchain_t *)((page)->dyn_win.dyn))

#define	DYN_PQOS_CMT_PROC(page) \
	((dyn_pqos_cmt_proc_t *)((page)->dyn_win.dyn))

#define	DYN_PQOS_MBM_PROC(page) \
	((dyn_pqos_mbm_proc_t *)((page)->dyn_win.dyn))

/* CPU unhalted cycles in a second */
extern uint64_t g_clkofsec;

/* Number of online CPUs */
extern int g_ncpus;

/* The sorting key */
extern int g_sortkey;

extern void win_fix_init(void);
extern void win_fix_fini(void);
extern void win_warn_msg(warn_type_t);
extern int win_dyn_init(void *);
extern void win_dyn_fini(void *);
extern void win_node_countvalue(node_t *, win_countvalue_t *);
extern void win_callchain_str_build(char *, int, int, void *);
extern void win_invalid_proc(void);
extern void win_invalid_lwp(void);
extern void win_note_show(char *);
extern void win_title_show(void);
extern boolean_t win_lat_data_show(track_proc_t *, dyn_lat_t *, boolean_t *);
extern lat_line_t* win_lat_buf_create(track_proc_t *, int, int *);
extern void win_lat_buf_fill(lat_line_t *, int, track_proc_t *,
    track_lwp_t *, int *);
extern int win_lat_cmp(const void *, const void *);
extern void win_lat_str_build(char *, int, int, void *);
extern void win_size2str(uint64_t, char *, int);
extern void win_callchain_line_get(win_reg_t *, int, char *, int);

#ifdef __cplusplus
}
#endif

#endif /* _NUMATOP_WIN_H */
