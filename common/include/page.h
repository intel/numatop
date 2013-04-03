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

#ifndef _NUMATOP_PAGE_H
#define	_NUMATOP_PAGE_H

#include <sys/types.h>
#include "types.h"
#include "util.h"
#include "win.h"
#include "cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	PAGE_WIN_TYPE(page) \
	((page)->dyn_win.type)

#define	PAGE_CMD(page) \
	(&((page)->cmd))

typedef struct _page {
	cmd_t cmd;
	struct _page *prev;
	struct _page *next;
	dyn_win_t dyn_win;
} page_t;

typedef struct _page_list {
	page_t *head;
	page_t *tail;
	page_t *cur;
	page_t *next_run;
	int npages;
} page_list_t;

extern int g_scr_height;
extern int g_scr_width;

extern void page_list_init(void);
extern void page_list_fini(void);
extern page_t *page_create(cmd_t *);
extern boolean_t page_next_execute(boolean_t);
extern page_t *page_current_get(void);
extern void page_next_set(page_t *);
extern page_t *page_current_set(page_t *);
extern void page_drop_next(page_t *);
extern page_t *page_curprev_get(void);
extern void page_win_destroy(void);
extern boolean_t page_smpl_start(page_t *page);

#ifdef __cplusplus
}
#endif

#endif /* _NUMATOP_PAGE_H */
