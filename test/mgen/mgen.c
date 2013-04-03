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
 * The application can generate LMA (Local Memory Access) and RMA
 * (Remote Memory Access) with latency information on NUMA system.
 *
 * Please note the latencies reported by mgen are not the official data
 * from Intel. It is just a tool to test numatop. 
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sched.h>
#include <pthread.h>
#include <sys/mman.h> 
#include <sys/shm.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <libgen.h>
#include <inttypes.h>
#include <string.h>
#include <signal.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/resource.h>

#ifndef PATH_MAX
#define	PATH_MAX	4096
#endif

#define CPU0_CPUFREQ_PATH \
	"/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq"

#define CPUINFO_PATH \
	"/proc/cpuinfo"

#define	MEAS_TIME_DEFAULT	5
#define	MAX_VALUE	4294967295U
#define	MHZ	1000000
#define GHZ	1000000000
#define	KHZ	1000
#define	NS_SEC	1000000000
#define	NS_MS	1000000
#define	MS_SEC	1000
#define MICROSEC	1000000
#define BUF_SIZE	256 * 1024 * 1024
#define RAND_ARRAY_SIZE	8192
#define INVALID_RAND	-1
#define LINE_SIZE	128
#define READ_NUM	10240000

static double s_nsofclk, s_clkofns;
static uint64_t s_clkofsec;
static int s_rand_arr[RAND_ARRAY_SIZE];
static double s_latest_avglat = 0.0;
static struct timeval s_tvbase;
static int s_ncpus;
static void *s_buf = NULL;
static unsigned int s_randseed;

static void *buf_create(int);
static void buf_release(void *);
static int dependent_read(void *, int, int, int);
static uint64_t rdtsc();

static void
print_usage(const char *exec_name)
{
	char buffer[PATH_MAX];

	strncpy(buffer, exec_name, PATH_MAX);
	buffer[PATH_MAX - 1] = 0;

	printf("Usage: %s [option(s)]\n", basename(buffer));
	printf("Options:\n"
	    "    -h: print helps\n"
	    "    -a: the cpu where allocates memory on the associated node\n"
	    "    -c: the cpu where creates a thread to access memory.\n"
	    "    -t: the seconds for measuring.\n"
	    "    -s: the random seed to build random address array (just for reproducing).\n");
	printf("\nFor example:\n"
	    "    1. Generate LMA (Local Memory Access) on cpu1 for 10s measuring:\n"
	    "       %s -a 1 -c 1 -t 10\n"
	    "    2. Generate RMA (Remote Memory Access) from cpu1 to cpu0 for 10s measuring:\n"
	    "       %s -a 0 -c 1 -t 10\n",
	    basename(buffer), basename(buffer));
}

/*
 * Get the interval in milliseconds between 2 timestamps.
 */
static unsigned int
msdiff(struct timeval *tva, struct timeval *tvb)
{
	time_t sdiff = tva->tv_sec - tvb->tv_sec;
	suseconds_t udiff = tva->tv_usec - tvb->tv_usec;

	if (sdiff < 0) {
		return (0);
	}

	if (udiff < 0) {
		udiff += MICROSEC;
		sdiff--;
	}

	if (sdiff < 0) {
		return (0);
	}

	if (sdiff >= (MAX_VALUE / MS_SEC)) {
		return ((unsigned int)MAX_VALUE);
	}

	return ((unsigned int)(sdiff * MS_SEC + udiff / MS_SEC));
}

/*
 * Get the current timestamp and convert it to milliseconds
 */
uint64_t
current_ms(void)
{
	struct timeval tvnow;

	(void) gettimeofday(&tvnow, 0);
	return (msdiff(&tvnow, &s_tvbase));
}

/*
 * Bind current thread to a cpu or unbind current thread
 * from a cpu.
 */
static int
processor_bind(int cpu)
{
	cpu_set_t cs;

	CPU_ZERO (&cs);
	CPU_SET (cpu, &cs);

	if (sched_setaffinity(0, sizeof (cs), &cs) < 0) {
		return (-1);
	}

	return (0);
}

static int
processor_unbind(void)
{
	cpu_set_t cs;
	int i;

	CPU_ZERO (&cs);
	for (i = 0; i < s_ncpus; i++) {
		CPU_SET (i, &cs);
	}

	if (sched_setaffinity(0, sizeof (cs), &cs) < 0) {
		return (-1);
	}

	return (0);
}

/*
 * Check the cpu name in proc info. Intel CPUs always have @ x.y
 * Ghz and that is the TSC frequency.
 */
static int
calibrate_cpuinfo(double *nsofclk, double *clkofns, uint64_t *clkofsec)
{
	FILE *f;
	char *line = NULL, unit[10];
	size_t len = 0;
	double freq = 0.0;

	if ((f = fopen(CPUINFO_PATH, "r")) == NULL) {
		return (-1);
	}

	while (getline(&line, &len, f) > 0) {
		if (strncmp(line, "model name", sizeof ("model name") - 1) != 0) {
			continue;
		}

		if (sscanf(line + strcspn(line, "@") + 1, "%lf%10s",
			&freq, unit) == 2) {
			if (strcasecmp(unit, "GHz") == 0) {
				freq *= GHZ;
			} else if (strcasecmp(unit, "Mhz") == 0) {
				freq *= MHZ;				
			}
			break;
		}
	}

	free(line);
	fclose(f);

	if (freq == 0.0) {
		return (-1);
	}

	*clkofsec = freq;
	*nsofclk = (double)NS_SEC / *clkofsec;
	*clkofns = (double)1.0 / *nsofclk;
	return (0);
}

/*
 * On all recent Intel CPUs, the TSC frequency is always
 * the highest p-state. So get that frequency from sysfs.
 * e.g. 2262000
 */
static int
calibrate_cpufreq(double *nsofclk, double *clkofns, uint64_t *clkofsec)
{
	int fd, i;
	char buf[32];
	uint64_t freq;

	if ((fd = open(CPU0_CPUFREQ_PATH, O_RDONLY)) < 0) {
		return (-1);
	}

	if ((i = read(fd, buf, sizeof (buf) - 1)) <= 0) {
		close(fd);
		return (-1);
	}

	close(fd);
	buf[i] = 0;
	if ((freq = atoll(buf)) == 0) {
		return (-1);
	}

	*clkofsec = freq * KHZ;
	*nsofclk = (double)NS_SEC / *clkofsec;
	*clkofns = (double)1.0 / *nsofclk;	
	return (0);
}

/*
 * Measure how many TSC cycles in a second and how many nanoseconds
 * in a TSC cycle.
 */
static void
calibrate_by_tsc(double *nsofclk, double *clkofns, uint64_t *clkofsec)
{
	uint64_t start_ms, end_ms, diff_ms;
	uint64_t start_tsc, end_tsc;

	/*
	 * Bind current thread to cpu0 to ensure the
	 * thread will not be migrated to another cpu
	 * while the rdtsc runs.
	 */
	if (processor_bind(0) != 0) {
		return;
	}

	/*
	 * Make sure the start_ms is at the beginning of
	 * one millisecond.
	 */
	end_ms = current_ms();
	while ((start_ms = current_ms()) == end_ms) {}

	start_tsc = rdtsc();
	while ((end_ms = current_ms()) < (start_ms + 100)) {}
	end_tsc = rdtsc();

	diff_ms = end_ms - start_ms;
	*nsofclk = (double)(diff_ms * NS_MS) /
	    (double)(end_tsc - start_tsc);
	*clkofns = (double)1.0 / *nsofclk;	
	*clkofsec = (uint64_t)((double)NS_SEC / *nsofclk);

	/*
	 * Unbind current thread from cpu0 once the measurement completed.
	 */
	(void) processor_unbind();
}

void
calibrate(void)
{
	if (calibrate_cpuinfo(&s_nsofclk, &s_clkofns, &s_clkofsec) == 0) {
		return;
	}
	
	if (calibrate_cpufreq(&s_nsofclk, &s_clkofns, &s_clkofsec) == 0) {
		return;	
	}

	calibrate_by_tsc(&s_nsofclk, &s_clkofns, &s_clkofsec);
}

static void
sigint_handler(int sig)
{
	switch (sig) {
	case SIGINT:
		(void) signal(SIGINT, sigint_handler);
		break;

	case SIGHUP:
		(void) signal(SIGINT, sigint_handler);
		break;
	
	case SIGQUIT:
		(void) signal(SIGINT, sigint_handler);
		break;

	case SIGPIPE:
		(void) signal(SIGINT, sigint_handler);
		break;

	case SIGTERM:
		(void) signal(SIGINT, sigint_handler);
		break;
	}

	printf("-------------------------\n");
	printf("%24s\n", "*** Terminated! ***");
	if (s_latest_avglat > 0.0) {
		printf("%9s  %13.1f\n\n", "Average", s_latest_avglat);
	} else {
		printf("%9s  %13.1f\n\n", "Average", 0.0);
	}

	if (s_buf != NULL) {
		buf_release(s_buf);
	}

	exit (0);
}

int
main(int argc, char *argv[])
{
	int cpu_alloc = -1, cpu_consumer = -1;
	int meas_sec = MEAS_TIME_DEFAULT;
	int ret = -1;
	char c;

	s_randseed = 0;
	optind = 1;
	opterr = 0;

	while ((c = getopt(argc, argv, "a:c:hf:t:s:")) != EOF) {
		switch (c) {
		case 'h':
			print_usage(argv[0]);
			ret = 0;
			goto L_EXIT0;

		case 'a':
			cpu_alloc = atoi(optarg);
			break;
			
		case 'c':
			cpu_consumer = atoi(optarg);
			break;
			
		case 't':
			meas_sec = atoi(optarg);
			break;

		case 's':
			s_randseed = atoi(optarg);
			break;

		case ':':
			printf("Missed argument for option %c.\n",
			    optopt);
			print_usage(argv[0]);
			goto L_EXIT0;

		case '?':
			printf("Unrecognized option %c.\n", optopt);
			print_usage(argv[0]);
			goto L_EXIT0;
		}
	}
	
	s_ncpus = sysconf(_SC_NPROCESSORS_CONF);

	if (cpu_alloc == -1) {
		printf("Missed argument for option '-a'.\n");
		print_usage(argv[0]);
		goto L_EXIT0;
	}

	if (cpu_consumer == -1) {
		printf("Missed argument for option '-c'.\n");
		print_usage(argv[0]);
		goto L_EXIT0;
	}

	if ((signal(SIGINT, sigint_handler) == SIG_ERR) ||
	    (signal(SIGHUP, sigint_handler) == SIG_ERR) ||
	    (signal(SIGQUIT, sigint_handler) == SIG_ERR) ||
	    (signal(SIGTERM, sigint_handler) == SIG_ERR) ||
	    (signal(SIGPIPE, sigint_handler) == SIG_ERR)) {
		goto L_EXIT0;	
	}

	gettimeofday(&s_tvbase, 0);
	calibrate();
	
	if ((s_buf = buf_create(cpu_alloc)) == NULL) {
		printf("Failed to create buffer.\n");
		goto L_EXIT0;
	}

	if (dependent_read(s_buf, cpu_consumer, cpu_alloc, meas_sec) != 0) {
		printf("Failed to dependent read.\n");
		goto L_EXIT0;
	}
	
	ret = 0;

L_EXIT0:
	if (s_buf != NULL) {
		buf_release(s_buf);
	}

	return (ret);
}

static int
last_free_elem(void)
{
	int i, cnt = 0;
	
	for (i = 0; i < RAND_ARRAY_SIZE; i++) {
		if (s_rand_arr[i] == INVALID_RAND) {
			cnt++;
			if (cnt > 1) {
				return (0);
			}	
		}
	}
	
	if (cnt == 1) {
		return (1);
	}
	
	return (0);	
}

static void
rand_array_init(void)
{
	int i, r, index = 0;

	if (s_randseed == 0) {
		s_randseed = time(0);
	}
	
	srand(s_randseed);
	
	for (i = 0; i < RAND_ARRAY_SIZE; i++) {
		s_rand_arr[i] = INVALID_RAND;
	}
	
	while (1) {
		for (;;) {
			r = rand() % RAND_ARRAY_SIZE;
			if (s_rand_arr[r] == INVALID_RAND) {
				break;
			}
		}
		
		if ((s_rand_arr[index] == INVALID_RAND) &&
		    (index != r)) {
			s_rand_arr[index] = r;
			index = r;
		}

		if (last_free_elem()) {
			s_rand_arr[index] = RAND_ARRAY_SIZE;
			break;
		}		
	}		
}

static void
rand_buf_init(void *buf, int size)
{
	int nblk = size / (RAND_ARRAY_SIZE * LINE_SIZE);
	int i, j;
	uint64_t **p, **blk_start, **end = NULL;
	
	p = (uint64_t **)buf;
	for (i = 0; i < nblk; i++) {
		blk_start = p;
		for (j = 0; j < RAND_ARRAY_SIZE; j++) {
			if (s_rand_arr[j] == RAND_ARRAY_SIZE) {
				end = p;
			}

			*p = (uint64_t *)((char *)blk_start + (s_rand_arr[j] * LINE_SIZE));
			p = (uint64_t **)((char *)p + LINE_SIZE);
		}
	}
	
	if (end != NULL) {
		*end = (uint64_t *)buf;	
	}
}

static void
buf_init(void *buf, int size)
{
	rand_array_init();
	rand_buf_init(buf, size);
}

static void *
buf_create(int cpu_alloc)
{
	void *buf;

	if (processor_bind(cpu_alloc) != 0) {
		return (NULL);		
	}

	buf= (void *)mmap(0, BUF_SIZE,
	    PROT_READ | PROT_WRITE | PROT_EXEC,MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (buf != NULL) {
		buf_init(buf, BUF_SIZE);
	}

	return (buf);
}

static void
buf_release(void *buf)
{
	(void) munmap(buf, BUF_SIZE);	
}

static uint64_t
rdtsc()
{	
	uint64_t var;
	uint32_t hi, lo;

	__asm volatile
	    ("rdtsc" : "=a" (lo), "=d" (hi));

	/* LINTED E_VAR_USED_BEFORE_SET */
	var = ((uint64_t)hi << 32) | lo;
	return (var);
}

static
void buf_read(void *buf, int read_num)
{
	asm  volatile (
		"xor %0, %0\n\t"
"LOOP1:\n\t"
		"mov (%1),%1\n\t"
		"inc %0\n\t"
		"cmp %2,%0\n\t"
		"jb LOOP1\n\t"
"STOP:\n\t"
		::"b"(0), "d"(buf), "r"(read_num)
	);
}

static void
latency_calculate(uint64_t count, uint64_t dur_cyc, uint64_t total_cyc)
{
	double sec, lat;
	
	sec = (double)total_cyc / (double)s_clkofsec;
	lat = ((double)dur_cyc * s_nsofclk) / (double)count;
	printf("%8.1fs  %13.1f\n", sec, lat);
	fflush(stdout);
}

static int
dependent_read(void *buf, int cpu_consumer, int cpu_alloc, int meas_sec)
{
	uint64_t total_count = 0, dur_count = 0;
	uint64_t start_tsc, end_tsc, prev_tsc;
	uint64_t run_cyc, total_cyc, dur_cyc;

	run_cyc = (uint64_t)((uint64_t)meas_sec * 
	    (uint64_t)((double)(NS_SEC) * s_clkofns));
	    
	if (processor_bind(cpu_consumer) != 0) {
		return (-1);
	}
	
	start_tsc = rdtsc();
	end_tsc = start_tsc;
	prev_tsc = start_tsc;

	fprintf(stdout, "\n!!! The reported latency is not the official data\n");
	fprintf(stdout, "    from Intel, it's just a tool to test numatop !!!\n");

	fprintf(stdout, "\nGenerating memory access from cpu%d to cpu%d for ~%ds ...\n",
	    cpu_consumer, cpu_alloc, meas_sec);

	fprintf(stdout, "(random seed to build random address array is %d.)\n", s_randseed);

	printf("\n%9s   %13s\n", "Time", "Latency(ns)");
	printf("-------------------------\n");

	while (1) {
		total_cyc = end_tsc - start_tsc;
		dur_cyc = end_tsc - prev_tsc;

		if (dur_cyc >= s_clkofsec) {
			latency_calculate(dur_count, dur_cyc, total_cyc);
			prev_tsc = rdtsc();
			dur_count = 0;
		}

		if (total_cyc >= run_cyc) {
			break;
		}

		if (total_count > 0) {
			s_latest_avglat = ((double)total_cyc * s_nsofclk) / (double)total_count;
		}

		buf_read(buf, READ_NUM);

		dur_count += READ_NUM;
		total_count += READ_NUM;
		end_tsc = rdtsc();
	}

	printf("-------------------------\n");

	if (total_count > 0) {
		printf("%9s  %13.1f\n\n", "Average",
		    ((double)total_cyc * s_nsofclk) / (double)total_count);
	} else {
		printf("%9s  %13.1f\n\n", "Average", 0.0);
	}

	return (0);
}

