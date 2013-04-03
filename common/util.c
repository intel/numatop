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

/* This file contains the helper routines. */

#define _GNU_SOURCE
#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <signal.h>
#include <stdarg.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <errno.h>
#include <limits.h>
#include "include/types.h"
#include "include/util.h"
#include "include/node.h"
#include "include/perf.h"

uint64_t g_clkofsec;

static int s_debuglevel;
static FILE *s_logfile;
static debug_ctl_t s_debug_ctl;
static dump_ctl_t s_dump_ctl;
static char s_exit_msg[EXIT_MSG_SIZE];
static double s_nsofclk;
static dump_ctl_t s_dump_ctl;

static unsigned int msdiff(struct timeval *, struct timeval *);

/*
 * Allocate a buffer and reset it to zero.
 */
void *
zalloc(size_t n)
{
	void *p;
	
	if (n == 0) {
		return (NULL);
	}

	if ((p = malloc(n)) != NULL) {
		(void) memset(p, 0, n);
	}

	return (p);
}

/*
 * Initialization for debug_print component.
 */
int
debug_init(int debug_level, FILE *log)
{
	(void) memset(s_exit_msg, 0, EXIT_MSG_SIZE);
	(void) memset(&s_debug_ctl, 0, sizeof (s_debug_ctl));
	if (pthread_mutex_init(&s_debug_ctl.mutex, NULL) != 0) {
		return (-1);
	}

	s_debug_ctl.inited = B_TRUE;
	s_logfile = log;
	s_debuglevel = debug_level;
	return (0);
}

/*
 * Clean up the resources for debug_print component.
 */
void
debug_fini(void)
{
	if (s_logfile != NULL) {
		(void) fclose(s_logfile);
	}

	if (s_debug_ctl.inited) {
		(void) pthread_mutex_destroy(&s_debug_ctl.mutex);
	}
}

/*
 * Write the message into log file according to different level.
 */
void
debug_print(FILE *out, int level, const char *fmt, ...)
{
	va_list ap;

	if (level <= s_debuglevel) {
		if (out == NULL) {
			if (s_logfile != NULL) {
				(void) pthread_mutex_lock(&s_debug_ctl.mutex);
				(void) fprintf(s_logfile,
				    "%lu: ", current_ms() / 1000);
				va_start(ap, fmt);
				(void) vfprintf(s_logfile, fmt, ap);
				va_end(ap);
				(void) fflush(s_logfile);
				(void) pthread_mutex_unlock(
				    &s_debug_ctl.mutex);
			}
		} else {
			(void) pthread_mutex_lock(&s_debug_ctl.mutex);
			(void) fprintf(out,
			    "%lu: ", current_ms() / 1000);
			va_start(ap, fmt);
			(void) vfprintf(out, fmt, ap);
			va_end(ap);
			(void) fflush(out);
			(void) pthread_mutex_unlock(&s_debug_ctl.mutex);
		}
	}
}

/*
 * Get the current timestamp and convert it to milliseconds
 * (timing from numatop startup).
 */
uint64_t
current_ms(void)
{
	struct timeval tvnow;

	(void) gettimeofday(&tvnow, 0);
	return (msdiff(&tvnow, &g_tvbase));
}

double
ratio(uint64_t value1, uint64_t value2)
{
	double r;

	if (value2 > 0) {
		r = (double)((double)value1 / (double)value2);
	} else {
		r = 0.0;
	}

	return (r);
}

static int
procfs_walk(char *path, int **id_arr, int *num)
{
	static DIR *dirp;
	struct dirent *dentp;
	int i = 0, size = *num, id;
	int *arr1 = *id_arr, *arr2;

	if ((dirp = opendir(path)) == NULL) {
		debug_print(NULL, 2, "Cannot open directory %s.\n", path);
		return (-1);
	}

	while ((dentp = readdir(dirp)) != NULL) {
		if (dentp->d_name[0] == '.') {
			/* skip "." and ".." */
			continue;
		}
		
		if ((id = atoi(dentp->d_name)) == 0) {
			/* Not a valid pid or lwpid.*/			
			continue;			 	
		}

		if (i >= size) {
			size = size << 1;
			if ((arr2 = realloc(arr1, size)) == NULL) {
				free(arr1);
				*id_arr = NULL;
				*num = 0;
				return (-1);
			}

			arr1 = arr2;
		}

		arr1[i] = id;
		i++;
	}
	
	*id_arr = arr1;
	*num = i;

	(void) closedir(dirp);
	return (0);
}

static int
procfs_enum_id(char *path, int **id_arr, int *nids)
{
	int *ids, num = PROCFS_ID_NUM;

	if ((ids = zalloc(PROCFS_ID_NUM * sizeof (int))) == NULL) {
		return (-1);
	}

	if (procfs_walk(path, &ids, &num) != 0) {
		if (ids != NULL) {
			free(ids);
		}

		return (-1);
	}

	*id_arr = ids;
	*nids = num;
	return (0);
}

/*
 * Retrieve the process's pid from '/proc'
 */
int
procfs_proc_enum(pid_t **pids, int *num)
{
	/*
	 * It's possible that the id in return buffer is 0, 
	 * the caller needs to check again.
	 */
	return (procfs_enum_id("/proc", (int **)pids, num));
}

/*
 * Retrieve the process's executable name from '/proc'
 */
int
procfs_pname_get(pid_t pid, char *buf, int size)
{
	char pname[PATH_MAX];
	int procfd; /* file descriptor for /proc/nnnnn/comm */
	int len;

	snprintf(pname, sizeof (pname), "/proc/%d/comm", pid);
	if ((procfd = open(pname, O_RDONLY)) < 0) {
		return (-1);
	}

	if ((len = read(procfd, buf, size)) < 0) {
		(void) close(procfd);
		return (-1);
	}

	buf[len - 1] = 0;
	(void) close(procfd);
	return (0);
}

/*
 * Retrieve the lwpid in process from '/proc'.
 */
int
procfs_lwp_enum(pid_t pid, int **lwps, int *num)
{
	char path[PATH_MAX];

	(void) snprintf(path, sizeof (path), "/proc/%d/task", pid);
	return (procfs_enum_id(path, lwps, num));
}

/*
 * Check if the specified pid can be found in '/proc'.
 */
boolean_t
procfs_proc_valid(pid_t pid)
{
	char pname[PATH_MAX];
	int procfd;

	(void) snprintf(pname, sizeof (pname), "/proc/%d/stat", pid);
	if ((procfd = open(pname, O_RDONLY)) < 0) {
		return (B_FALSE);
	}

	(void) close(procfd);
	return (B_TRUE);
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

void
exit_msg_put(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void) vsnprintf(s_exit_msg, EXIT_MSG_SIZE, fmt, ap);
	va_end(ap);
}

void
exit_msg_print(void)
{
	if (strlen(s_exit_msg) > 0) {
		(void) printf("%s", s_exit_msg);
	}
}

/*
 * Convert the CPU cycles to nanoseconds.
 */
uint64_t
cyc2ns(uint64_t cyc)
{
	uint64_t ns;

	ns = (uint64_t)(cyc * s_nsofclk);
	return (ns);
}

static inline void
cpuid(unsigned int *eax, unsigned int *ebx, unsigned int *ecx,
	unsigned int *edx)
{
	 __asm__ __volatile__ (
	 	"cpuid\n\t"
	 	: "=a" (*eax),
	 	  "=b" (*ebx),
	 	  "=c" (*ecx),
	 	  "=d" (*edx)
		: "a" (*eax)
	);
}

/*
 * Get the CPU type.
 */
cpu_type_t
cpu_type_get(void)
{
	unsigned int eax, ebx, ecx, edx;
	int family, model, ext_model;
	cpu_type_t type = CPU_UNSUP;
	char vendor[16];
	
	eax = 0;
	cpuid(&eax, &ebx, &ecx, &edx);
	
	strncpy(&vendor[0], (char *)(&ebx), 4);
	strncpy(&vendor[4], (char *)(&ecx), 4);
	strncpy(&vendor[8], (char *)(&edx), 4);
	vendor[12] = 0;

	if (strncmp(vendor, "Genu" "ntel" "ineI", 12) != 0) {
		return (CPU_UNSUP);
	}
	
	eax = 1;
	cpuid(&eax, &ebx, &ecx, &edx);

	family = CPU_FAMILY(eax);
	model = CPU_MODEL(eax);
	ext_model = CPU_EXT_MODEL(eax);

	if (family == 6) {
		model = (ext_model << 4) + model;
		switch (model) {
		case 26:
			type = CPU_NHM_EP;
			break;			

		case 44:
			type = CPU_WSM_EP;
			break;

		case 45:
			type = CPU_SNB_EP;
			break;
			
		case 46:
			type = CPU_NHM_EX;
			break;

		case 47:
			type = CPU_WSM_EX;
			break;
		}
	}

	return (type);
}

/*
 * Get the TSC cycles.
 */
#ifdef __x86_64__
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
#else
static uint64_t
rdtsc()
{
	uint64_t var;

	__asm volatile
	    ("rdtsc" : "=A" (var));

	return (var);
}
#endif

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
		debug_print(NULL, 2, "Fail to bind to CPU%d\n", cpu);
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
	for (i = 0; i < g_ncpus; i++) {
		CPU_SET (i, &cs);
	}

	if (sched_setaffinity(0, sizeof (cs), &cs) < 0) {
		debug_print(NULL, 2, "Fail to unbind from CPU\n");
		return (-1);
	}

	return (0);
}

/*
 * Check the cpu name in proc info. Intel CPUs always have @ x.y
 * Ghz and that is the TSC frequency.
 */
static int
calibrate_cpuinfo(double *nsofclk, uint64_t *clkofsec)
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

	debug_print(NULL, 2, "calibrate_cpuinfo: nsofclk = %.4f, "
	    "clkofsec = %lu\n", *nsofclk, *clkofsec);

	return (0);
}

/*
 * On all recent Intel CPUs, the TSC frequency is always
 * the highest p-state. So get that frequency from sysfs.
 * e.g. 2262000
 */
static int
calibrate_cpufreq(double *nsofclk, uint64_t *clkofsec)
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

	debug_print(NULL, 2, "calibrate_cpufreq: nsofclk = %.4f, "
	    "clkofsec = %lu\n", *nsofclk, *clkofsec);

	return (0);
}

/*
 * Measure how many TSC cycles in a second and how many
 * nanoseconds in a TSC cycle.
 */
static void
calibrate_by_tsc(double *nsofclk, uint64_t *clkofsec)
{
	uint64_t start_ms, end_ms, diff_ms;
	uint64_t start_tsc, end_tsc;
	int i;

	for (i = 0; i < g_ncpus; i++) {
		/*
		 * Bind current thread to cpuN to ensure the
		 * thread can not be migrated to another cpu
		 * while the rdtsc runs.
		 */
		if (processor_bind(i) == 0) {
			break;
		}
	}

	if (i == g_ncpus) {
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

	*clkofsec = (uint64_t)((double)NS_SEC / *nsofclk);

	/*
	 * Unbind current thread from cpu once the measurement completed.
	 */
	processor_unbind();

	debug_print(NULL, 2, "calibrate_by_tsc: nsofclk = %.4f, "
	    "clkofsec = %lu\n", *nsofclk, *clkofsec);
}

void
calibrate(void)
{
	if (calibrate_cpuinfo(&s_nsofclk, &g_clkofsec) == 0) {
		return;
	}
	
	if (calibrate_cpufreq(&s_nsofclk, &g_clkofsec) == 0) {
		return;	
	}

	calibrate_by_tsc(&s_nsofclk, &g_clkofsec);
}

/*
 * Initialization for dump control structure.
 */
int
dump_init(FILE *dump_file)
{
	(void) memset(&s_dump_ctl, 0, sizeof (s_dump_ctl));
	if ((s_dump_ctl.fout = dump_file) != NULL) {
		if ((s_dump_ctl.cache =
		    zalloc(DUMP_CACHE_SIZE)) == NULL) {
			return (-1);
		}

		s_dump_ctl.pcur = s_dump_ctl.cache;
		s_dump_ctl.rest_size = DUMP_CACHE_SIZE;
	}

	return (0);
}

/*
 * Clean up resources of dump control structure.
 */
void
dump_fini(void)
{
	if (s_dump_ctl.fout != NULL) {
		(void) fclose(s_dump_ctl.fout);
	}

	if (s_dump_ctl.cache != NULL) {
		free(s_dump_ctl.cache);
	}
}

/*
 * Write the message into dump file.
 */
void
dump_write(const char *fmt, ...)
{
	va_list ap;
	int n;

	if (s_dump_ctl.fout == NULL) {
		return;
	}

	if (!s_dump_ctl.cache_mode) {
		va_start(ap, fmt);
		(void) vfprintf(s_dump_ctl.fout, fmt, ap);
		va_end(ap);
		(void) fflush(s_dump_ctl.fout);
	} else {
		va_start(ap, fmt);
		n = vsnprintf(s_dump_ctl.pcur,
		    s_dump_ctl.rest_size, fmt, ap);
		va_end(ap);
		s_dump_ctl.pcur += n;
		s_dump_ctl.rest_size -= n;
	}
}

void
dump_cache_enable(void)
{
	s_dump_ctl.cache_mode = B_TRUE;
}

void
dump_cache_disable(void)
{
	s_dump_ctl.cache_mode = B_FALSE;
}

void
dump_cache_flush(void)
{
	if (s_dump_ctl.fout != NULL) {
		(void) fprintf(s_dump_ctl.fout, "%s", s_dump_ctl.cache);
		(void) fflush(s_dump_ctl.fout);

		(void) memset(s_dump_ctl.cache, 0, DUMP_CACHE_SIZE);
		s_dump_ctl.pcur = s_dump_ctl.cache;
		s_dump_ctl.rest_size = DUMP_CACHE_SIZE;
		s_dump_ctl.cache_mode = B_FALSE;
	}
}

/*
 * Print the message to STDERR.
 */
void
stderr_print(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	/* LINTED E_SEC_PRINTF_VAR_FMT */
	(void) vfprintf(stderr, format, ap);
	(void) fprintf(stderr, "\r");
	va_end(ap);
}

static boolean_t
int_get(char *str, int *digit)
{
	char *end;
	long val;

	/* Distinguish success/failure after strtol */
	errno = 0;
	val = strtol(str, &end, 10);
	if (((errno == ERANGE) && ((val == LONG_MAX) || (val == LONG_MIN))) ||
		((errno != 0) && (val == 0))) {
		return (B_FALSE);
	}

	if (end == str) {
		return (B_FALSE);
	}	

	*digit = val;
	return (B_TRUE);
}

/*
 * The function is only called for processing small digits. 
 * For example, if the string is "0-9", extract the digit 0 and 9.
 */
static boolean_t
hyphen_int_extract(char *str, int *start, int *end)
{
	char tmp[DIGIT_LEN_MAX];

	if (strlen(str) >= DIGIT_LEN_MAX) {
		return (B_FALSE);
	}

	if (sscanf(str, "%511[^-]", tmp) <= 0) {
		return (B_FALSE);
	} 
   
   	if (!int_get(tmp, start)) {
   		return (B_FALSE);
   	}
   
	if (sscanf(str, "%*[^-]-%511s", tmp) <= 0) {
		return (B_FALSE);
	}

   	if (!int_get(tmp, end)) {
   		return (B_FALSE);
   	}

	return (B_TRUE);
}

static boolean_t
arrary_add(int *arr, int arr_size, int index, int value, int num)
{
	int i;
	
	if ((index >= arr_size) || ((index + num) > arr_size)) {
		return (B_FALSE);
	}

	for (i = 0; i < num; i++) {
		arr[index + i] = value + i;
	}

	return (B_TRUE);
}

/*
 * Extract the digits from string. For example:
 * "1-2,5-7"	return 1 2 5 6 7 in "arr".
 */
static boolean_t
str_int_extract(char *str, int *arr, int arr_size, int *num)
{
	char *p, *cur, *scopy;
	int start, end, total = 0;
	int len = strlen(str);
	boolean_t ret = B_FALSE;

	if ((scopy = malloc(len + 1)) == NULL) {
		return (B_FALSE);
	}

	strncpy(scopy, str, len);
	scopy[len] = 0;
	cur = scopy;

	while (cur < (scopy + len)) {
		if ((p = strchr(cur, ',')) != NULL) {
			*p = 0;
		}

		if (strchr(cur, '-') != NULL) {
			if (hyphen_int_extract(cur, &start, &end)) {
				if (arrary_add(arr, arr_size, total, start, end - start + 1)) {
					total += end - start + 1;
				} else {
					goto L_EXIT;
				}
			}
		} else {
			if (int_get(cur, &start)) {
				if (arrary_add(arr, arr_size, total, start, 1)) {
					total++;
				} else {
					goto L_EXIT;	
     				}
			}
		}

		if (p != NULL) {
			cur = p + 1;
		} else {
			break;
		}
	}

	*num = total;
	ret = B_TRUE;

L_EXIT:	
	free(scopy);
	return (ret);
}

static boolean_t
file_int_extract(char *path, int *arr, int arr_size, int *num)
{
	FILE *fp;
	char buf[LINE_SIZE];
	
	if ((fp = fopen(path, "r")) == NULL) {
		return (B_FALSE);
	}

	if (fgets(buf, LINE_SIZE, fp) == NULL) {
		fclose(fp);
		return (B_FALSE);
	}

	fclose(fp);
	return (str_int_extract(buf, arr, arr_size, num));
}

boolean_t
sysfs_node_enum(int *node_arr, int arr_size, int *num)
{
	return (file_int_extract(NODE_NONLINE_PATH, node_arr, arr_size, num));
}

boolean_t
sysfs_cpu_enum(int nid, int *cpu_arr, int arr_size, int *num)
{
	char path[PATH_MAX];
	
	snprintf(path, PATH_MAX, "%s/node%d/cpulist", NODE_INFO_ROOT, nid);
	return (file_int_extract(path, cpu_arr, arr_size, num));
}

static boolean_t
memsize_parse(char *str, uint64_t *size)
{
	char *p;
	char tmp[DIGIT_LEN_MAX];
	
	if ((p = strchr(str, ':')) == NULL) {
		return (B_FALSE);
	}

	++p;
	if (sscanf(p, "%*[^0-9]%511[0-9]", tmp) <= 0) {
		return (B_FALSE);
	}

	*size = strtoll(tmp, NULL, 10) * KB_BYTES;
	return (B_TRUE);
}

boolean_t
sysfs_meminfo(int nid, meminfo_t *info)
{
	FILE *fp;
	char path[PATH_MAX];
	char *line = NULL;
	size_t len = 0;
	boolean_t ret = B_FALSE;
	int num = sizeof (meminfo_t) / sizeof (uint64_t), i = 0;

	memset(info, 0, sizeof (meminfo_t));	
	snprintf(path, PATH_MAX, "%s/node%d/meminfo", NODE_INFO_ROOT, nid);
	if ((fp = fopen(path, "r")) == NULL) {
		return (B_FALSE);
	}

	while ((getline(&line, &len, fp) > 0) && (i < num)) {
		if (strstr(line, "MemTotal:") != NULL) {
			if (!memsize_parse(line, &info->mem_total)) {
				goto L_EXIT;
			}
			i++;
			continue;
		}

		if (strstr(line, "MemFree:") != NULL) {
			if (!memsize_parse(line, &info->mem_free)) {
				goto L_EXIT;
			}
			i++;
			continue;
		}

		if (strstr(line, "Active:") != NULL) {
			if (!memsize_parse(line, &info->active)) {
				goto L_EXIT;
			}
			i++;
			continue;
		}

		if (strstr(line, "Inactive:") != NULL) {			
			if (!memsize_parse(line, &info->inactive)) {
				goto L_EXIT;
			}
			i++;
			continue;
		}

		if (strstr(line, "Dirty:") != NULL) {
			if (!memsize_parse(line, &info->dirty)) {
				goto L_EXIT;
			}
			i++;
			continue;
		}

		if (strstr(line, "Writeback:") != NULL) {
			if (!memsize_parse(line, &info->writeback)) {
				goto L_EXIT;
			}
			i++;
			continue;
		}

		if (strstr(line, "Mapped:") != NULL) {
			if (!memsize_parse(line, &info->mapped)) {
				goto L_EXIT;
			}
			i++;
			continue;
		}
	}

	ret = B_TRUE;

L_EXIT:
	if (line != NULL) {
		free(line);
	}

	fclose(fp);
	return (ret);
}

int
ulimit_expand(int newfd)
{
	struct rlimit rlim;
	
	if (getrlimit(RLIMIT_NOFILE, &rlim) != 0) {
		return (-1);
	}
	
	if ((rlim.rlim_cur == RLIM_INFINITY) &&
		(rlim.rlim_max == RLIM_INFINITY)) {
		return (0);
	}
	
	if (rlim.rlim_cur != RLIM_INFINITY) {
		rlim.rlim_cur += PERF_FD_NUM;
	}

	if (rlim.rlim_max != RLIM_INFINITY) {
		rlim.rlim_max += PERF_FD_NUM;
	}

	if (setrlimit(RLIMIT_NOFILE, &rlim) != 0) {
		return (-1);	
	}
	
	return (0);
}

int
array_alloc(void **pp, int *ncur, int *nmax, int size, int num)
{
	int i;
	void *p;

	if (*ncur == *nmax) {
		if (*pp == NULL) {
			if ((*pp = zalloc(num * size)) == NULL) {
				return (-1);
			}

			*nmax = num;			
		} else {
			i = (*nmax) << 1;
			if ((p = realloc(*pp, i * size)) == NULL) {
				if (*pp != NULL) {
					free(*pp);
					*pp = NULL;
				}

				return (-1);	
			}

			*pp = p;
			*nmax = i;			
		}
	}
	
	return (0);
}

void
pagesize_init(void)
{
	g_pagesize = getpagesize();
}
