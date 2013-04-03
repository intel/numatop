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

/* This file contains code to handle the display part of NumaTOP */

#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <curses.h>
#include "include/types.h"
#include "include/util.h"
#include "include/lwp.h"
#include "include/proc.h"
#include "include/disp.h"
#include "include/node.h"
#include "include/page.h"
#include "include/cmd.h"
#include "include/win.h"

static disp_ctl_t s_disp_ctl;
static cons_ctl_t s_cons_ctl;

static int disp_start(void);
static void* disp_handler(void *);
static void* cons_handler(void *);

/*
 * Initialization for the display control structure.
 */
static int
disp_ctl_init(void)
{
	(void) memset(&s_disp_ctl, 0, sizeof (s_disp_ctl));
	if (pthread_mutex_init(&s_disp_ctl.mutex, NULL) != 0) {
		return (-1);
	}

	if (pthread_cond_init(&s_disp_ctl.cond, NULL) != 0) {
		(void) pthread_mutex_destroy(&s_disp_ctl.mutex);
		return (-1);
	}

	s_disp_ctl.inited = B_TRUE;
	return (0);
}

/*
 * Clean up the resources of display control structure.
 */
static void
disp_ctl_fini(void)
{
	if (s_disp_ctl.inited) {
		(void) pthread_mutex_destroy(&s_disp_ctl.mutex);
		(void) pthread_cond_destroy(&s_disp_ctl.cond);
		s_disp_ctl.inited = B_FALSE;
	}
}

/*
 * Initialization for the display control structure and
 * creating 'disp thread'.
 */
int
disp_init(void)
{
	if (disp_ctl_init() != 0) {
		return (-1);
	}

	if (disp_start() != 0) {
		disp_ctl_fini();
		return (-1);
	}

	return (0);
}

/*
 * Before free the resources of display control structure,
 * make sure the 'disp thread' and 'cons thread' quit yet.
 */
void
disp_fini(void)
{
	disp_ctl_fini();
}

/*
 * Initialization for the console control structure.
 */
int
disp_cons_ctl_init(void)
{
	(void) memset(&s_cons_ctl, 0, sizeof (s_cons_ctl));
	if (pipe(s_cons_ctl.pipe) < 0) {
		return (-1);
	}

	s_cons_ctl.inited = B_TRUE;
	return (0);
}

/*
 * Clean up the resources of console control structure.
 */
void
disp_cons_ctl_fini(void)
{
	if (s_cons_ctl.inited) {
		(void) close(s_cons_ctl.pipe[0]);
		(void) close(s_cons_ctl.pipe[1]);
		s_cons_ctl.inited = B_FALSE;
	}
}

/*
 * Send a character '<PIPE_CHAR_QUIT>' to pipe to notify the
 * 'cons thread' to quit.
 */
void
disp_consthr_quit(void)
{
	char c;

	debug_print(NULL, 2, "Send PIPE_CHAR_QUIT to cons thread\n");
	c = PIPE_CHAR_QUIT;
	(void) write(s_cons_ctl.pipe[1], &c, 1);
	(void) pthread_join(s_cons_ctl.thr, NULL);
	debug_print(NULL, 2, "cons thread exit yet\n");
}

static void
dispthr_flagset_nolock(disp_flag_t flag)
{
	s_disp_ctl.flag = flag;
	(void) pthread_cond_signal(&s_disp_ctl.cond);
}

static void
dispthr_flagset_lock(disp_flag_t flag)
{
	(void) pthread_mutex_lock(&s_disp_ctl.mutex);
	dispthr_flagset_nolock(flag);
	(void) pthread_mutex_unlock(&s_disp_ctl.mutex);
}

/*
 * Notify 'disp thread' that the perf profiling data is ready.
 */
void
disp_profiling_data_ready(int intval_ms)
{
	(void) pthread_mutex_lock(&s_disp_ctl.mutex);
	s_disp_ctl.intval_ms = intval_ms;
	dispthr_flagset_nolock(DISP_FLAG_PROFILING_DATA_READY);
	(void) pthread_mutex_unlock(&s_disp_ctl.mutex);
}

/*
 * Notify 'disp thread' that the perf profiling data is failed.
 */
void
disp_profiling_data_fail(void)
{
	dispthr_flagset_lock(DISP_FLAG_PROFILING_DATA_FAIL);
}

/*
 * Notify 'disp thread' that the perf load latency data is ready.
 */
void
disp_ll_data_ready(int intval_ms)
{
	(void) pthread_mutex_lock(&s_disp_ctl.mutex);
	s_disp_ctl.intval_ms = intval_ms;
	dispthr_flagset_nolock(DISP_FLAG_LL_DATA_READY);
	(void) pthread_mutex_unlock(&s_disp_ctl.mutex);
}

/*
 * Notify 'disp thread' that the perf load latency data is failed.
 */
void
disp_ll_data_fail(void)
{
	dispthr_flagset_lock(DISP_FLAG_LL_DATA_FAIL);
}

/*
 * The handler of signal 'SIGWINCH'. The function sends a 'refresh'
 * command to 'cons thread' to let it do a refresh operation.
 */
/* ARGSUSED */
void
disp_on_resize(int sig)
{
	char c = PIPE_CHAR_RESIZE;

	(void) write(s_cons_ctl.pipe[1], &c, 1);
}

void
disp_intval(char *buf, int size)
{
	(void) snprintf(buf, size, "%.1fs",
	    (float)(s_disp_ctl.intval_ms) / (float)MS_SEC);
}

/*
 * Create 'disp thread' and 'cons thread'.
 */
static int
disp_start(void)
{
	if (pthread_create(&s_cons_ctl.thr, NULL, cons_handler, NULL) != 0) {
		debug_print(NULL, 2, "Create cons thread failed.\n");
		return (-1);
	}

	if (pthread_create(&s_disp_ctl.thr, NULL, disp_handler, NULL) != 0) {
		debug_print(NULL, 2, "Create disp thread failed.\n");
		disp_consthr_quit();
		return (-1);
	}

	return (0);
}

/*
 * Send the 'HOME' command to 'disp thread'.
 */
static void
submit_cmd_home(void)
{
	(void) pthread_mutex_lock(&s_disp_ctl.mutex);
	CMD_ID_SET(&s_disp_ctl.cmd, CMD_HOME_ID);
	dispthr_flagset_nolock(DISP_FLAG_CMD);
	(void) pthread_mutex_unlock(&s_disp_ctl.mutex);
}

static void
timeout_set(struct timespec *timeout, int nsec)
{
	struct timeval tv;

	(void) gettimeofday(&tv, NULL);
	timeout->tv_sec = tv.tv_sec + nsec;
	timeout->tv_nsec = tv.tv_usec * 1000;
}

/*
 * The common entry for processing command.
 */
static void
cmd_received(cmd_t *cmd, boolean_t *quit, struct timespec *timeout)
{
	boolean_t badcmd, execute;
	int cmd_id = CMD_ID(cmd);

	execute = B_TRUE;
	*quit = B_FALSE;

	switch (cmd_id) {
	case CMD_QUIT_ID:
		*quit = B_TRUE;
		return;

	case CMD_RESIZE_ID:		
		/*
		 * The screen resize signal would trigger this
		 * command. Destroy existing screen and curses
		 * resources and then re-init the curses resources.
		 */
		win_fix_fini();
		page_win_destroy();
		reg_curses_fini();
		if (reg_curses_init(B_FALSE)) {
			win_fix_init();
		} else {
			execute = B_FALSE;
		}

		timeout_set(timeout, DISP_DEFAULT_INTVAL);
		break;

	case CMD_REFRESH_ID:		
		/*
		 * User hit the hotkey 'R' to refresh current window.
		 */
		timeout_set(timeout, DISP_DEFAULT_INTVAL);
		break;
	}

	if (execute) {
		cmd_execute(cmd, &badcmd);
	}
}

/*
 * Called when user hits the 'UP'/'DOWN' key.
 */
static void
key_scroll(int scroll_type)
{
	page_t *page;
	dyn_win_t *dyn_win;

	if ((page = page_current_get()) != NULL) {
		dyn_win = &page->dyn_win;
		if (dyn_win->scroll != NULL) {
			(dyn_win->scroll)(dyn_win, scroll_type);
		}
	}
}

/*
 * Called when user hits the 'ENTER' key.
 */
static void
scroll_enter(void)
{
	page_t *page;
	dyn_win_t *dyn_win;

	if ((page = page_current_get()) != NULL) {
		dyn_win = &page->dyn_win;
		if (dyn_win->scroll_enter != NULL) {
			(dyn_win->scroll_enter)(dyn_win);
		}
	}
}

static boolean_t
consthr_init_wait(void)
{
	struct timespec timeout;
	disp_flag_t flag;
	int status = 0;

	timeout_set(&timeout, DISP_DEFAULT_INTVAL);
	(void) pthread_mutex_lock(&s_disp_ctl.mutex);
	flag = s_disp_ctl.flag;

	/*
	 * The cons thread issues a command to go to home window
	 * when it completes the initialization.
	 */
	while ((flag != DISP_FLAG_CMD) &&
	    (flag != DISP_FLAG_QUIT)) {
		status = pthread_cond_timedwait(&s_disp_ctl.cond,
		    &s_disp_ctl.mutex, &timeout);

		if (status == ETIMEDOUT) {
			break;
		}

		flag = s_disp_ctl.flag;
	}

	(void) pthread_mutex_unlock(&s_disp_ctl.mutex);
	if ((status == ETIMEDOUT) || (flag == DISP_FLAG_QUIT)) {
		return (B_FALSE);
	}

	return (B_TRUE);
}

/*
 * The handler of 'disp thread'.
 */
/* ARGSUSED */
static void *
disp_handler(void *arg)
{
	disp_flag_t flag;
	int status = 0;
	cmd_t cmd;
	boolean_t quit, pagelist_inited = B_FALSE;
	struct timespec timeout;
	boolean_t job_started;
	page_t *cur;
	uint64_t start_ms, diff_ms;

	/*
	 * Wait cons thread to complete initialization.
	 */
	if (!consthr_init_wait()) {
		debug_print(NULL, 2,
		    "Timeout for waiting cons thread to complete initialization\n");

		/*
		 * The cons thread should exit with error or startup failed, 
		 * disp thread stops running.
		 */
		goto L_EXIT;
	}

	/*
	 * NumaTOP contains multiple windows. It uses double linked list
	 * to link all of windows.
	 */
	page_list_init();
	pagelist_inited = B_TRUE;

	timeout_set(&timeout, 0);
	start_ms = current_ms();

	for (;;) {
		status = 0;
		(void) pthread_mutex_lock(&s_disp_ctl.mutex);
		flag = s_disp_ctl.flag;
		while (flag == DISP_FLAG_NONE) {
			status = pthread_cond_timedwait(&s_disp_ctl.cond,
			    &s_disp_ctl.mutex, &timeout);
			flag = s_disp_ctl.flag;
			if (status == ETIMEDOUT) {
				break;
			}
		}

		if (flag == DISP_FLAG_CMD) {
			(void) memcpy(&cmd, &s_disp_ctl.cmd, sizeof (cmd));
		}

		s_disp_ctl.flag = DISP_FLAG_NONE;
		(void) pthread_mutex_unlock(&s_disp_ctl.mutex);

		diff_ms = current_ms() - start_ms;
		if (g_run_secs <= diff_ms / MS_SEC) {
			g_run_secs = TIME_NSEC_MAX;
			debug_print(NULL, 2,
			    "disp: it's time to exit\n");
			continue;
		}

		if ((status == ETIMEDOUT) && (flag == DISP_FLAG_NONE)) {
			if ((cur = page_current_get()) == NULL) {
				timeout_set(&timeout, DISP_DEFAULT_INTVAL);
				continue;
			}

			/*
			 * Notify the perf thread to start sampling.
			 */
			job_started = page_smpl_start(cur);

			/*
			 * It's a 'refresh' operation, set the current page to next page.
			 */
			page_next_set(cur);
			if (!job_started) {
				/*
				 * Just refresh the current page.
				 */
				page_next_execute(B_FALSE);
			}

			timeout_set(&timeout, DISP_DEFAULT_INTVAL);
			continue;
		}

		switch (flag) {
		case DISP_FLAG_QUIT:
			debug_print(NULL, 2,
			    "disp: received DISP_FLAG_QUIT\n");
			goto L_EXIT;

		case DISP_FLAG_CMD:
			cmd_received(&cmd, &quit, &timeout);
			if (quit) {
				debug_print(NULL, 2, "disp thread received CMD_QUIT_ID\n");
				goto L_EXIT;
			}

			break;

		case DISP_FLAG_PROFILING_DATA_READY:			
			/*
			 * Show the page.
			 */
			page_next_execute(B_FALSE);
			timeout_set(&timeout, DISP_DEFAULT_INTVAL);
			break;

		case DISP_FLAG_PROFILING_DATA_FAIL:
			/*
			 * Received the notification that the perf profiling
			 * was failed.
			 */
			debug_print(NULL, 2,
			    "disp: received DISP_FLAG_PROFILING_DATA_FAIL.\n");
			win_warn_msg(WARN_PERF_DATA_FAIL);
			submit_cmd_home();
			break;

		case DISP_FLAG_LL_DATA_READY:
			page_next_execute(B_FALSE);
			timeout_set(&timeout, DISP_DEFAULT_INTVAL);
			break;
			
		case DISP_FLAG_LL_DATA_FAIL:
			break;		

		case DISP_FLAG_SCROLLUP:
			/*
			 * User hits the "UP" key.
			 */
			key_scroll(SCROLL_UP);
			if (status == ETIMEDOUT) {
				timeout_set(&timeout, DISP_DEFAULT_INTVAL);
			}
			break;

		case DISP_FLAG_SCROLLDOWN:
			/*
			 * User hits the "DOWN" key.
			 */
			key_scroll(SCROLL_DOWN);
			if (status == ETIMEDOUT) {
				timeout_set(&timeout, DISP_DEFAULT_INTVAL);
			}
			break;

		case DISP_FLAG_SCROLLENTER:
			/*
			 * User selects a scroll item and hit the "ENTER".
			 */
			scroll_enter();
			if (status == ETIMEDOUT) {
				timeout_set(&timeout, DISP_DEFAULT_INTVAL);
			}
			break;
			
		default:
			break;
		}
	}

L_EXIT:
	if (pagelist_inited) {
		page_list_fini();
	}

	/*
	 * Let the perf thread exit first.
	 */
	perf_fini();

	debug_print(NULL, 2, "disp thread is exiting\n");
	return (NULL);
}

/*
 * The handler of 'cons thread'
 */
/* ARGSUSED */
static void *
cons_handler(void *arg)
{
	char c, ch;
	int cmd_id;

	if (!reg_curses_init(B_TRUE)) {
		goto L_EXIT;
	}

	win_fix_init();

	/*
	 * Excute "home" command. It shows the NumaTop default page.
	 */
	submit_cmd_home();

	for (;;) {
		FD_ZERO(&s_cons_ctl.fds);
		FD_SET(STDIN_FILENO, &s_cons_ctl.fds);
		FD_SET(s_cons_ctl.pipe[0], &s_cons_ctl.fds);

		/*
		 * Wait one character from "stdin" or pipe.
		 */
		if (select(s_cons_ctl.pipe[0] + 1, &s_cons_ctl.fds,
		    NULL, NULL, NULL) > 0) {
			if (FD_ISSET(s_cons_ctl.pipe[0], &s_cons_ctl.fds)) {
				(void) read(s_cons_ctl.pipe[0], &ch, 1);

				/*
				 * Character is from pipe.
				 */
				if (ch == PIPE_CHAR_QUIT) {
					/*
					 * Received a QUIT notification,
					 * "console thread" will be quit
					 */
					debug_print(NULL, 2, "cons: "
					    "received PIPE_CHAR_QUIT\n");
					break;
				}

				if (ch == PIPE_CHAR_RESIZE) {
					/*
					 * Send the "RESIZE" command
					 * to "display thread".
					 */
					(void) pthread_mutex_lock(
					    &s_disp_ctl.mutex);

					CMD_ID_SET(&s_disp_ctl.cmd,
					    CMD_RESIZE_ID);
					dispthr_flagset_nolock(DISP_FLAG_CMD);

					(void) pthread_mutex_unlock(
					    &s_disp_ctl.mutex);
				}
			} else {
				/*
				 * Character is from STDIN.
				 */
				c = getch();
				ch = tolower(c);
				dump_write("\n<-- User hit the key '%c' "
				    "(ascii = %d) -->\n", c, (int)c);

				cmd_id = cmd_id_get(ch);
				if (cmd_id != CMD_INVALID_ID) {
					/*
					 * The character is a command. Send
					 * the command to 'disp thread'.
					 */
					(void) pthread_mutex_lock(
					    &s_disp_ctl.mutex);

					CMD_ID_SET(&s_disp_ctl.cmd, cmd_id);
					dispthr_flagset_nolock(DISP_FLAG_CMD);

					(void) pthread_mutex_unlock(
					    &s_disp_ctl.mutex);
				} else {
					/*
					 * Hit the keys 'UP'/'DOWN'/'ENTER'
					 */
					switch (ch) {
					case 2:		/* key-down */
						dispthr_flagset_lock(
						    DISP_FLAG_SCROLLDOWN);
						break;

					case 3:		/* key-up */
						dispthr_flagset_lock(
						    DISP_FLAG_SCROLLUP);
						break;

					case 13:	/* enter. */
						dispthr_flagset_lock(
						    DISP_FLAG_SCROLLENTER);
						break;

					default:
						break;
					}
				}
			}
		}
	}

	reg_curses_fini();

L_EXIT:
	debug_print(NULL, 2, "cons thread is exiting\n");
	return (NULL);
}

void
disp_dispthr_quit_wait(void)
{
	(void) pthread_join(s_disp_ctl.thr, NULL);
	debug_print(NULL, 2, "disp thread exit yet\n");
}

void
disp_dispthr_quit_start(void)
{
	(void) pthread_mutex_lock(&s_disp_ctl.mutex);
	CMD_ID_SET(&s_disp_ctl.cmd, CMD_QUIT_ID);
	dispthr_flagset_nolock(DISP_FLAG_CMD);
	(void) pthread_mutex_unlock(&s_disp_ctl.mutex);
}
