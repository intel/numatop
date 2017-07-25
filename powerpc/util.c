/*
 * Copyright (c) 2017, IBM Corporation
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "../common/include/os/os_util.h"

/* PowerPC has Time Base (TB) register similar to intel TSC. */
uint64_t
rdtsc(void)
{
	uint64_t tb = 0;

	__asm__ volatile("mftb %0" : "=r"(tb));

	return (tb);
}

int
arch__cpuinfo_freq(double *freq, char *unit)
{
	FILE *f;
	char *line = NULL, *c;
	size_t len = 0;
	int ret = -1;

	if ((f = fopen(CPUINFO_PATH, "r")) == NULL) {
		return -1;
	}

	while (getline(&line, &len, f) > 0) {
		if (strncmp(line, "clock", sizeof ("clock") -1)) {
			continue;
		}

		if (sscanf(line + strcspn(line, ":") + 1, "%lf%10s",
			freq, unit) == 2) {
			if (strcasecmp(unit, "GHz") == 0) {
				*freq *= GHZ;
			} else if (strcasecmp(unit, "MHz") == 0) {
				*freq *= MHZ;
			}
			break;
		}
	}

	/*
	 * Hyperwiser does not expose cpufreq on PowerVMs(pSeries)
	 * servers. Thus 'clock' field from /proc/cpuinfo shows
	 * absolute max freq. While in case of PowerNV servers,
	 * 'clock' field shows current freq for each individual
	 * processor.
	 *
	 * Use 'clock' field to get freq on pSeries and fallback to
	 * sysfs cpufreq approach for PowerNV.
	 */
	while (getline(&line, &len, f) > 0) {
		if (strncmp(line, "platform", sizeof("sizeof") -1)) {
			continue;
		}

		c = strchr(line, ':');
		if (c - line + 2 < len &&
		    !strncmp(c + 2, "pSeries", sizeof ("pSeries") - 1)) {
			ret = 0;
			break;
		}
	}

	free(line);
	fclose(f);
	return ret;
}
