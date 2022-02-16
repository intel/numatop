/*
 * Copyright (c) 2013, Intel Corporation
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
#include <stdint.h>
#include "../include/util.h"
#include "../../../common/include/util.h"

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

void
arch__dependent_read(void *buf, int meas_sec)
{
	uint64_t total_count = 0, dur_count = 0;
	uint64_t start_tsc, end_tsc, prev_tsc;
	uint64_t run_cyc, total_cyc, dur_cyc;

	printf("\n%9s   %13s\n", "Time", "Latency(ns)");
	printf("-------------------------\n");

	run_cyc = (uint64_t)((uint64_t)meas_sec *
	    (uint64_t)((double)(NS_SEC) * (1.0 / s_nsofclk)));

	start_tsc = rdtsc();
	end_tsc = start_tsc;
	prev_tsc = start_tsc;

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
}
