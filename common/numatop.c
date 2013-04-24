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

/* This file contains main(). */

#include <inttypes.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <libgen.h>
#include "include/types.h"
#include "include/util.h"
#include "include/proc.h"
#include "include/node.h"
#include "include/disp.h"
#include "include/plat.h"
#include "include/perf.h"
#include "include/sym.h"

static void sigint_handler(int sig);
static void print_usage(const char *exec_name);

int g_ncpus;
int g_sortkey;
precise_type_t g_precise;
pid_t g_numatop_pid;
struct timeval g_tvbase;
int g_run_secs;
int g_pagesize;

/*
 * The main function.
 */
int
main(int argc, char *argv[])
{
	int ret = 1, debug_level = 0;
	FILE *log = NULL, *dump = NULL;
	char c;

	/*
	 * The numatop requires some authorities to setup perf and increase the hard limit of
	 * fd. So either the user runs as root or following condtions need to be satisfied.
	 *
	 * 1. Set "-1" in /proc/sys/kernel/perf_event_paranoid to let the non-root be able to
	 *    setup perf. e.g.
	 *    echo -1 > /proc/sys/kernel/perf_event_paranoid
	 *
	 * 2. The CAP_SYS_RESOURCE capability is required to user for increasing the hard
	 *    limit of fd.
	 */

	g_sortkey = SORT_KEY_CPU;
	g_precise = PRECISE_NORMAL;
	g_numatop_pid = getpid();
	g_run_secs = TIME_NSEC_MAX;
	optind = 1;
	opterr = 0;

	/*
	 * Parse command line arguments.
	 */
	while ((c = getopt(argc, argv, "d:l:o:f:t:hf:s:")) != EOF) {
		switch (c) {
		case 'h':
			print_usage(argv[0]);
			ret = 0;
			goto L_EXIT0;

		case 'l':
			debug_level = atoi(optarg);
			if ((debug_level < 0) || (debug_level > 2)) {
				stderr_print("Invalid log_level %d.\n",
				    debug_level);
				print_usage(argv[0]);
				goto L_EXIT0;
			}
			break;

		case 'f':
			if (optarg == NULL) {
				stderr_print("Invalid output file.\n");
				goto L_EXIT0;
			}

			if ((log = fopen(optarg, "w")) == NULL) {
				stderr_print("Cannot open '%s' for writing.\n",
				    optarg);
				goto L_EXIT0;
			}
			break;

		case 's':
			if (optarg == NULL) {
				print_usage(argv[0]);
				goto L_EXIT0;
			}

			if (strcasecmp(optarg, "high") == 0) {
				g_precise = PRECISE_HIGH;
				break;
			}

			if (strcasecmp(optarg, "low") == 0) {
				g_precise = PRECISE_LOW;
				break;
			}

			if (strcasecmp(optarg, "normal") == 0) {
				g_precise = PRECISE_NORMAL;
				break;
			}

			stderr_print("Invalid sampling_precision '%s'.\n",
			    optarg);
			print_usage(argv[0]);
			goto L_EXIT0;

		case 'd':
			if (optarg == NULL) {
				stderr_print("Invalid dump file.\n");
				goto L_EXIT0;
			}

			if ((dump = fopen(optarg, "w")) == NULL) {
				stderr_print("Cannot open '%s' for dump.\n",
				    optarg);
				goto L_EXIT0;
			}
			break;

		case 't':
			g_run_secs = atoi(optarg);
			if (g_run_secs <= 0) {
				stderr_print("Invalid run time %d.\n",
				    g_run_secs);
				print_usage(argv[0]);
				goto L_EXIT0;
			}
			break;

		case ':':
			stderr_print("Missed argument for option %c.\n",
			    optopt);
			print_usage(argv[0]);
			goto L_EXIT0;

		case '?':
			stderr_print("Unrecognized option %c.\n", optopt);
			print_usage(argv[0]);
			goto L_EXIT0;
		}
	}

	if (plat_detect() != 0) {
		stderr_print("CPU is not supported!\n");
		ret = 2;
		goto L_EXIT0;
	}

	/*
	 * It could be failed if user doesn't have authority.
	 */
	(void) ulimit_expand(PERF_FD_NUM);

	/*
	 * Get the number of online cores in system.
	 */
	if ((g_ncpus = sysfs_online_ncpus()) == -1) {
		stderr_print("Platform is not supported "
		    "(numatop supports up to %d CPUs)\n", NCPUS_MAX);
		goto L_EXIT0;
	}

	pagesize_init();
	gettimeofday(&g_tvbase, 0);

	if (debug_init(debug_level, log) != 0) {
		goto L_EXIT1;
	}

	debug_print(NULL, 2, "Detected %d online CPU.\n", g_ncpus);
	log = NULL;
	sym_init();

	if (dump_init(dump) != 0) {
		goto L_EXIT2;
	}

	dump = NULL;

	/*
	 * Calculate how many nanoseconds for a TSC cycle.
	 */
	calibrate();

	/*
	 * Initialize for the "window-switching" table.
	 */
	switch_table_init();

	if (proc_group_init() != 0) {
		goto L_EXIT3;
	}

	if (node_group_init() != 0) {
		stderr_print("The node/cpu number is out of range, \n"
			"numatop supports up to %d nodes and %d CPUs\n",
			NNODES_MAX, NCPUS_MAX);
		goto L_EXIT4;
	}

	if (disp_cons_ctl_init() != 0) {
		goto L_EXIT5;
	}

	/*
	 * Catch signals from terminal.
	 */
	if ((signal(SIGINT, sigint_handler) == SIG_ERR) ||
	    (signal(SIGHUP, sigint_handler) == SIG_ERR) ||
	    (signal(SIGQUIT, sigint_handler) == SIG_ERR) ||
	    (signal(SIGTERM, sigint_handler) == SIG_ERR) ||
	    (signal(SIGPIPE, sigint_handler) == SIG_ERR)) {
		goto L_EXIT6;
	}

	/*
	 * Initialize the perf sampling facility.
	 */
	if (perf_init() != 0) {
		debug_print(NULL, 2, "perf_init() is failed\n");
		goto L_EXIT6;
	}

	/*
	 * Initialize for display and create console thread & display thread.
	 */
	if (disp_init() != 0) {
		perf_fini();
		goto L_EXIT6;
	}

	/*
	 * Wait the disp thread to exit. The disp thread would
	 * exit when user hits the hotkey 'Q' or press "CTRL+C".
	 */
	disp_dispthr_quit_wait();

	/*
	 * Notify cons thread to exit.
	 */
	disp_consthr_quit();
	
	disp_fini();
	stderr_print("NumaTOP is exiting ...\n");
	(void) fflush(stdout);
	ret = 0;

L_EXIT6:
	disp_cons_ctl_fini();

L_EXIT5:
	node_group_fini();

L_EXIT4:
	proc_group_fini();

L_EXIT3:
	dump_fini();

L_EXIT2:
	sym_fini();
	debug_fini();

L_EXIT1:
	exit_msg_print();

L_EXIT0:
	if (dump != NULL) {
		(void) fclose(dump);
	}

	if (log != NULL) {
		(void) fclose(log);
	}

	return (ret);
}

/*
 * The signal handler.
 */
static void
sigint_handler(int sig)
{
	switch (sig) {
	case SIGINT:
		(void) signal(SIGINT, sigint_handler);
		break;

	case SIGHUP:
		(void) signal(SIGHUP, sigint_handler);
		break;

	case SIGQUIT:
		(void) signal(SIGQUIT, sigint_handler);
		break;

	case SIGPIPE:
		(void) signal(SIGPIPE, sigint_handler);
		break;

	case SIGTERM:
		(void) signal(SIGTERM, sigint_handler);
		break;

	default:
		return;
	}

	/*
	 * It's same as the operation when user hits the hotkey 'Q'.
	 */
	disp_dispthr_quit_start();
}

/*
 * Print command-line help information.
 */
static void
print_usage(const char *exec_name)
{
	char buffer[PATH_MAX];

	strncpy(buffer, exec_name, PATH_MAX);
	buffer[PATH_MAX - 1] = 0;

	stderr_print("Usage: %s [option(s)]\n", basename(buffer));
	stderr_print(
	    "  -h    print help\n"
	    "  -d    path of the file to save the data in screen\n"
	    "  -l    0/1/2, the level of output warning message\n"
	    "  -f    path of the file to save warning message.\n"
	    "        e.g. numatop -l 2 -f /tmp/warn.log.\n"
	    "  -s    sampling precision: \n"
	    "        normal: balance precision and overhead (default)\n"
	    "        high  : high sampling precision\n"
	    "                (high overhead, not recommended option)\n"
	    "        low   : low sampling precision, suitable for high load system\n");
}
