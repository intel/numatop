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

#ifndef _NUMATOP_REG_H
#define	_NUMATOP_REG_H

#include <sys/types.h>
#include <sys/time.h>
#include <inttypes.h>
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	ALIGN_LEFT = 0,
	ALIGN_MIDDLE
} reg_align_t;

typedef enum {
	SCROLL_DOWN = 0,
	SCROLL_UP
} reg_scroll_t;

typedef struct _scroll_line {
	boolean_t enabled;	/* indicate if support "scrolling" */
	int highlight;	/* current highlight line. */
	int page_start;
} scroll_line_t;

typedef struct _win_reg {
	void *hdl;
	int begin_x;	/* offset to stdscr */
	int begin_y;	/* offset to stdscr */
	int ncols_scr;
	int nlines_scr;
	unsigned int mode;
	int nlines_total;
	void *buf;
	void (*line_get)(struct _win_reg *, int, char *, int);
	scroll_line_t scroll;
} win_reg_t;

extern int reg_init(win_reg_t *, int, int, int, int, unsigned int);
extern void reg_buf_init(win_reg_t *, void *,
	void (*line_get)(win_reg_t *, int, char *, int));
extern void reg_scroll_init(win_reg_t *, boolean_t);
extern void reg_erase(win_reg_t *);
extern void reg_refresh(win_reg_t *);
extern void reg_refresh_nout(win_reg_t *);
extern void reg_update_all(void);
extern void reg_win_destroy(win_reg_t *seg);
extern void reg_line_write(win_reg_t *, int, reg_align_t, char *);
extern void reg_highlight_write(win_reg_t *, int, int, char *);
extern void reg_line_scroll(win_reg_t *, int);
extern void reg_scroll_show(win_reg_t *, void *, int,
	void (*str_build_func)(char *, int, int, void *));
extern boolean_t reg_curses_init(boolean_t);
extern void reg_curses_fini(void);

#ifdef __cplusplus
}
#endif

#endif /* _NUMATOP_REG_H */
