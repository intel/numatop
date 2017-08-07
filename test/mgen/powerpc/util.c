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

extern uint64_t current_ms(struct timeval *);

static
void buf_read(void *buf, int read_num)
{
	asm  volatile (
		"mtctr %1\n\t"
"LOOP1:\n\t"
		"ld %0,0(%0)\n\t"
		"bdnz LOOP1\n\t"
"STOP:\n\t"
		:: "r"(buf), "r"(read_num)
	);
}

static void
latency_calculate(uint64_t count, uint64_t dur_ms, uint64_t total_ms)
{
	double sec, lat;

	sec = (double)total_ms / (double)MS_SEC;
	lat = ((double)dur_ms * NS_MS) / (double)count;
	printf("%8.1fs  %13.1f\n", sec, lat);
	fflush(stdout);
}

void
arch__dependent_read(void *buf, int meas_sec)
{
	uint64_t total_count = 0, dur_count = 0;
	uint64_t start_ms, end_ms, prev_ms;
	uint64_t run_ms, total_ms, dur_ms;

	printf("\n%9s   %13s\n", "Time", "Latency(ns)");
	printf("-------------------------\n");

	run_ms = (uint64_t)meas_sec * MS_SEC;

	start_ms = current_ms(&s_tvbase);
	prev_ms = start_ms;
	end_ms = start_ms;

	while (1) {
		total_ms = end_ms - start_ms;
		dur_ms = end_ms - prev_ms;

		if (dur_ms >= MS_SEC) {
			latency_calculate(dur_count, dur_ms, total_ms);
			prev_ms = current_ms(&s_tvbase);
			dur_count = 0;
		}

		if (total_ms >= run_ms) {
			break;
		}

		if (total_count > 0) {
			s_latest_avglat = ((double)total_ms * NS_MS) /
						(double)total_count;
		}

		buf_read(buf, READ_NUM);

		dur_count += READ_NUM;
		total_count += READ_NUM;
		end_ms = current_ms(&s_tvbase);
	}

	printf("-------------------------\n");

	if (total_count > 0) {
		printf("%9s  %13.1f\n\n", "Average",
			((double)total_ms * NS_MS) / (double)total_count);
	} else {
		printf("%9s  %13.1f\n\n", "Average", 0.0);
	}
}
