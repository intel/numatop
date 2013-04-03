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

/*
 * numatop uses libcurses to display data on screen. For better control,
 * numatop brings a new definition 'reg' (regin) to control the data on
 * screen. It locates where the data is, what the attributes it has
 * (e.g. font, color, scrolling enable?, ...).
 * This file contains code to handle the 'reg'.
 */

#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <curses.h>
#include <signal.h>
#include "include/types.h"
#include "include/reg.h"
#include "include/win.h"
#include "include/disp.h"

int g_scr_height;
int g_scr_width;

static boolean_t s_curses_init = B_FALSE;

/*
 * Highlight the selected line.
 */
static void
reg_idx_highlight(win_reg_t *reg, int idx)
{
	scroll_line_t *scroll = &reg->scroll;
	char line[WIN_LINECHAR_MAX];

	if (reg->hdl != NULL) {
		reg->line_get(reg, idx, line, WIN_LINECHAR_MAX);
		reg_highlight_write(reg, idx - scroll->page_start,
		    ALIGN_LEFT, line);
	}
}

/*
 * Display the hidden lines on screen.
 */
static void
reg_hidden_show(win_reg_t *reg, int idx_start)
{
	int i, idx_end;
	char line[WIN_LINECHAR_MAX];

	if (reg->hdl == NULL) {
		return;
	}

	if ((idx_end = idx_start + reg->nlines_scr) > reg->nlines_total) {
		idx_end = reg->nlines_total;
	}

	reg_erase(reg);
	for (i = idx_start; i < idx_end; i++) {
		reg->line_get(reg, i, line, WIN_LINECHAR_MAX);
		reg_line_write(reg, i - idx_start, ALIGN_LEFT, line);
	}
}

/*
 * Lowlight the selected line.
 */
static void
reg_idx_lowlight(win_reg_t *reg, int idx)
{
	scroll_line_t *scroll = &reg->scroll;
	char line[WIN_LINECHAR_MAX];

	if (reg->hdl != NULL) {
		reg->line_get(reg, idx, line, WIN_LINECHAR_MAX);
		reg_line_write(reg, idx - scroll->page_start,
		    ALIGN_LEFT, line);
	}
}

/*
 * Create a 'window'.
 */
static WINDOW *
reg_win_create(win_reg_t *reg)
{
	return (subwin(stdscr, reg->nlines_scr, reg->ncols_scr,
	    reg->begin_y, reg->begin_x));
}

/*
 * Initialization for one 'reg'.
 */
int
reg_init(win_reg_t *reg, int begin_x, int begin_y, int ncols, int nlines,
	unsigned int mode)
{
	if ((ncols <= 0) || (nlines <= 0)) {
		return (-1);
	}

	(void) memset(reg, 0, sizeof (win_reg_t));
	reg->begin_x = begin_x;
	reg->begin_y = begin_y;
	reg->ncols_scr = ncols;
	reg->nlines_scr = nlines;
	reg->mode = mode;
	reg->hdl = reg_win_create(reg);
	return (reg->begin_y + reg->nlines_scr);
}

/*
 * Initialization for the data buffer in 'reg'.
 */
void
reg_buf_init(win_reg_t *reg, void *buf,
	void (*line_get)(win_reg_t *, int, char *, int))
{
	reg->buf = buf;
	reg->line_get = line_get;
}

/*
 * Initialization for 'scrolling'.
 */
void
reg_scroll_init(win_reg_t *reg, boolean_t enable)
{
	scroll_line_t *scroll = &reg->scroll;

	scroll->enabled = enable;
	scroll->highlight = -1;
}

/*
 * Erase the data in 'reg' on screen.
 */
void
reg_erase(win_reg_t *reg)
{
	if (reg->hdl != NULL) {
		(void) werase(reg->hdl);
	}
}

/*
 * Refresh the data in 'reg' and display the update data on screen.
 */
void
reg_refresh(win_reg_t *reg)
{
	if (reg->hdl != NULL) {
		(void) wrefresh(reg->hdl);
	}
}

/*
 * Refresh the data in 'reg' but doesn't display the update data on
 * screen immediately.
 */
void
reg_refresh_nout(win_reg_t *reg)
{
	if (reg->hdl != NULL) {
		(void) wnoutrefresh(reg->hdl);
	}
}

/*
 * Update the data on screen immediately.
 */
void
reg_update_all(void)
{
	(void) doupdate();
}

/*
 * Free the resource of libcurses 'window'.
 */
void
reg_win_destroy(win_reg_t *reg)
{
	if (reg->hdl != NULL) {
		(void) delwin(reg->hdl);
		reg->hdl = NULL;
	}
}

/*
 * Fill data in a line and display the line on screen.
 */
void
reg_line_write(win_reg_t *reg, int line, reg_align_t align, char *content)
{
	int pos_x = 0, len;

	if (reg->hdl == NULL) {
		return;
	}

	if (reg->mode != 0) {
		(void) wattron(reg->hdl, reg->mode);
	}

	len = strlen(content);
	if (align == ALIGN_MIDDLE) {
		pos_x = (reg->ncols_scr - len) / 2;
	}

	if (len > 0) {
		(void) mvwprintw(reg->hdl, line, pos_x, content);
	}

	if (reg->mode != 0) {
		wattroff(reg->hdl, reg->mode);
	}
}

/*
 * Fill data in one line and display it on screen with highlight.
 */
void
reg_highlight_write(win_reg_t *reg, int line, int align, char *content)
{
	int pos_x = 0, len;

	if (reg->hdl == NULL) {
		return;
	}

	(void) wattron(reg->hdl, A_REVERSE | A_BOLD);
	len = strlen(content);
	if (align == ALIGN_MIDDLE) {		
		pos_x = (reg->ncols_scr - len) / 2;
		ASSERT(pos_x >= 0);
	}

	if (len > 0) {
		(void) mvwprintw(reg->hdl, line, pos_x, content);
	}
	
	wattroff(reg->hdl, A_REVERSE | A_BOLD);
}

/*
 * Scroll one line UP/DOWN.
 */
void
reg_line_scroll(win_reg_t *reg, int scroll_type)
{
	scroll_line_t *scroll = &reg->scroll;
	int highlight, idx_next;
	boolean_t page_scroll = B_FALSE;

	if ((!scroll->enabled) || (reg->hdl == NULL)) {
		return;
	}

	if ((highlight = scroll->highlight) == -1) {
		highlight = 0;
	}

	if (scroll_type == SCROLL_UP) {
		if ((idx_next = highlight - 1) < 0) {
			return;
		}

		if (idx_next < scroll->page_start) {
			scroll->page_start--;
			page_scroll = B_TRUE;
		}
	} else if (scroll_type == SCROLL_DOWN) {
		if ((idx_next = highlight + 1) >= reg->nlines_total) {
			return;
		}

		if (((idx_next - scroll->page_start) % reg->nlines_scr) == 0) {
			scroll->page_start++;
			page_scroll = B_TRUE;
		}
	} else {
		return;
	}

	if (page_scroll) {
		reg_hidden_show(reg, scroll->page_start);
	}

	reg_idx_lowlight(reg, highlight);
	reg_idx_highlight(reg, idx_next);
	scroll->highlight = idx_next;
	reg_refresh(reg);
}

/*
 * Show the 'scrolling reg'.
 */
void
reg_scroll_show(win_reg_t *reg, void *lines, int nreqs,
    void (*str_build_func)(char *, int, int, void *))
{
	int highlight, i, start, end;
	char content[WIN_LINECHAR_MAX];

	highlight = reg->scroll.highlight;
	if (highlight != -1) {
		if (highlight >= reg->scroll.page_start) {
			if ((i = ((highlight - reg->scroll.page_start) /
			    reg->nlines_scr)) != 0) {
				reg->scroll.page_start += reg->nlines_scr * i;
			}
		} else {
			reg->scroll.page_start =
			    (highlight / reg->nlines_scr) *
			    reg->nlines_scr;
		}

		start = reg->scroll.page_start;
		i = MIN(nreqs, reg->nlines_scr);
		if ((end = start + i) > reg->nlines_total) {
			end = reg->nlines_total;
		}
	} else {
		highlight = 0;
		start = 0;
		end = MIN(nreqs, reg->nlines_scr);
	}

	for (i = start; i < end; i++) {
		str_build_func(content, sizeof (content), i, lines);
		dump_write("%s\n", content);
		if (i != highlight) {
			reg_line_write(reg, i - reg->scroll.page_start,
			    ALIGN_LEFT, content);
		}
	}

	if ((highlight >= start) && (highlight < end)) {
		str_build_func(content, sizeof (content), highlight, lines);
		reg_highlight_write(reg, highlight - reg->scroll.page_start,
			ALIGN_LEFT, content);
		reg->scroll.highlight = highlight;
	}
}

/*
 * Clean up the resources for libcurses.
 */
void
reg_curses_fini(void)
{
	if (s_curses_init) {
		(void) clear();
		(void) refresh();
		(void) endwin();
		(void) fflush(stdout);
		(void) putchar('\r');
		s_curses_init = B_FALSE;		
	}
}

/*
 * Initialization for libcurses.
 */
boolean_t
reg_curses_init(boolean_t first_load)
{
	(void) initscr();
	(void) refresh();
	(void) start_color();	
	(void) keypad(stdscr, TRUE);
	(void) nonl();
	(void) cbreak();
	(void) noecho();
	(void) curs_set(0);
	(void) use_default_colors();

	getmaxyx(stdscr, g_scr_height, g_scr_width);

	/*
	 * Set a window resize signal handler.
	 */
	(void) signal(SIGWINCH, disp_on_resize);
	s_curses_init = B_TRUE;

	if ((g_scr_height < 24 || g_scr_width < 80)) {
		if (!first_load) {
			(void) mvwprintw(stdscr, 0, 0,
			    "Terminal size is too small.");
			(void) mvwprintw(stdscr, 1, 0,
			    "Please resize it to 80x24 or larger.");
			(void) refresh();
		} else {
			reg_curses_fini();
			stderr_print("Terminal size is too small "
			    "(resize it to 80x24 or larger).\n");
		}

		dump_write("\n%s\n", "Terminal size is too small.");
		dump_write("%s\n", "Please resize it to 80x24 or larger.");
		return (B_FALSE);
	}

	return (B_TRUE);
}

void
reg_win_clear(void)
{
	if (s_curses_init) {
		(void) clear();
		(void) refresh();
	}
}
