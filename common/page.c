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

/* This file contains code to handle the 'page' which is used in display */

#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include "include/types.h"
#include "include/cmd.h"
#include "include/page.h"
#include "include/win.h"
#include "include/perf.h"
#include "include/os/node.h"
#include "include/os/os_page.h"

static page_list_t s_page_list;

/*
 * Free the resource of a page.
 */
static void
page_free(page_t *page)
{
	if (page != NULL) {
		win_dyn_fini(page);
		free(page);
	}
}

/*
 * Initialization for page list.
 */
void
page_list_init(void)
{
	s_page_list.head = NULL;
	s_page_list.tail = NULL;
	s_page_list.cur = NULL;
	s_page_list.next_run = NULL;
	s_page_list.npages = 0;
}

/*
 * Clean up the resources of the page list.
 */
void
page_list_fini(void)
{
	page_t *p1, *p2;

	p1 = s_page_list.head;
	while (p1 != NULL) {
		p2 = p1->next;
		page_free(p1);
		p1 = p2;
	}

	s_page_list.head = NULL;
	s_page_list.tail = NULL;
	s_page_list.cur = NULL;
	s_page_list.next_run = NULL;
	s_page_list.npages = 0;
}

/*
 * Append a new page to the tail of page list.
 */
static void
page_append(page_t *page)
{
	page_t *tail;

	page->prev = page->next = NULL;
	if ((tail = s_page_list.tail) != NULL) {
		tail->next = page;
		page->prev = tail;
	} else {
		s_page_list.head = page;
	}

	s_page_list.tail = page;
	s_page_list.npages++;
}

/*
 * Allocate the resource for the new page.
 */
page_t *
page_create(cmd_t *cmd)
{
	page_t *page;

	if ((page = zalloc(sizeof (page_t))) == NULL) {
		return (NULL);
	}

	/*
	 * Copy the command information in page.
	 */
	(void) memcpy(&page->cmd, cmd, sizeof (cmd_t));

	/*
	 * Drop all the pages after the current one.
	 */
	page_drop_next(s_page_list.cur);

	/*
	 * Append the new page after the current one.
	 */
	page_append(page);
	s_page_list.next_run = page;
	return (page);
}

/*
 * Show the page on the screen.
 */
static boolean_t
page_show(page_t *page, boolean_t smpl)
{
	if (g_scr_height < 24 || g_scr_width < 80) {
		dump_write("\n%s\n", "Terminal size is too small.");
		dump_write("%s\n", "Please resize it to 80x24 or larger.");
		return (B_FALSE);
	}

	if (node_group_refresh(B_FALSE) != 0) {
		return (B_FALSE);
	}

	if ((!page->dyn_win.inited) &&
	    (win_dyn_init(page) != 0)) {
		return (B_FALSE);
	}

	if (smpl) {		
		win_warn_msg(WARN_WAIT);
		(void) os_page_smpl_start(page);
		return (B_TRUE);
	}

	return (page->dyn_win.draw(&page->dyn_win));
}

/*
 * Show the next page in list.
 */
boolean_t
page_next_execute(boolean_t smpl)
{
	page_t *next_run;
	boolean_t ret;

	if ((next_run = s_page_list.next_run) == NULL) {
		return (B_FALSE);
	}

	ret = page_show(next_run, smpl);
	s_page_list.cur = next_run;

	if (smpl) {
		s_page_list.next_run = next_run;
	} else {
		s_page_list.next_run = NULL;
	}

	return (ret);
}

page_t *
page_current_get(void)
{
	return (s_page_list.cur);
}

page_t *
page_current_set(page_t *page)
{
	s_page_list.cur = page;
	return (s_page_list.cur);
}

void
page_next_set(page_t *page)
{
	s_page_list.next_run = page;
}

/*
 * Free all the pages which are after the specified page node in list.
 */
void
page_drop_next(page_t *page)
{
	page_t *next, *p;

	if (page == NULL) {
		return;
	}

	next = page->next;
	while (next != NULL) {
		p = next->next;
		page_free(next);
		s_page_list.npages--;
		next = p;
	}

	page->next = NULL;
	s_page_list.tail = page;
}

/*
 * Get the previous node of current one in list.
 */
page_t *
page_curprev_get(void)
{
	page_t *cur;

	if ((cur = s_page_list.cur) != NULL) {
		return (cur->prev);
	}

	return (NULL);
}

/*
 * Free the resources of windows which are associated with the pages in list.
 */
void
page_win_destroy(void)
{
	page_t *p1;

	p1 = s_page_list.head;
	while (p1 != NULL) {
		win_dyn_fini(p1);
		p1 = p1->next;
	}
}


