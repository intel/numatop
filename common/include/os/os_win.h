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

#ifndef _NUMATOP_OS_WIN_H
#define	_NUMATOP_OS_WIN_H

#include <sys/types.h>
#include <inttypes.h>
#include "../types.h"
#include "../proc.h"
#include "node.h"
#include "os_perf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	NOTE_LAT \
	"Q: Quit; H: Home; B: Back; R: Refresh; " \
	"C: Call-Chain; D: Distribution"

#define	NOTE_LATNODE \
	"Q: Quit; H: Home; B: Back; R: Refresh"

#define NOTE_LLCALLCHAIN	\
	"Q: Quit; H: Home; B: Back; R: Refresh"	

struct _nodeoverview_line;
struct _dyn_nodedetail;
struct _dyn_callchain;
struct _dyn_win;
struct _page;
struct _lat_line;

extern void os_nodeoverview_caption_build(char *, int);
extern void os_nodeoverview_data_build(char *, int,
    struct _nodeoverview_line *, node_t *);
extern void os_nodedetail_data(struct _dyn_nodedetail *, win_reg_t *);
extern int os_callchain_list_show(struct _dyn_callchain *, track_proc_t *,
    track_lwp_t *);
extern void os_lat_buf_hit(struct _lat_line *, int, os_perf_llrec_t *,
	uint64_t *, uint64_t *);
extern boolean_t os_lat_win_draw(struct _dyn_win *);
extern void* os_llcallchain_dyn_create(struct _page *);
extern void os_llcallchain_win_destroy(struct _dyn_win *);
extern boolean_t os_llcallchain_win_draw(struct _dyn_win *);
extern void os_llcallchain_win_scroll(struct _dyn_win *, int);
extern boolean_t os_latnode_win_draw(struct _dyn_win *);

#ifdef __cplusplus
}
#endif

#endif /* _NUMATOP_OS_WIN_H */
