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

#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include "../include/types.h"
#include "../include/cmd.h"
#include "../include/page.h"
#include "../include/perf.h"
#include "../include/os/os_page.h"

/*
 * Start sampling the performance data.
 */
boolean_t
os_page_smpl_start(page_t *page)
{
	cmd_t *cmd = PAGE_CMD(page);

	switch (CMD_ID(cmd)) {
	case CMD_HOME_ID:
		/* fall through */
	case CMD_IR_NORMALIZE_ID:
		/* fall through */
	case CMD_MONITOR_ID:
		/* fall through */
	case CMD_LWP_ID:
		/* fall through */
	case CMD_NODE_OVERVIEW_ID:
		/* fall through */
	case CMD_NODE_DETAIL_ID:
		/* fall through */
	case CMD_CALLCHAIN_ID:
		if (perf_profiling_smpl() == 0) {
			return (B_TRUE);
		}
		break;

	case CMD_LAT_ID:
		/* fall through */
	case CMD_LLCALLCHAIN_ID:
		/* fall through */
	case CMD_LATNODE_ID:
		/* fall through */
	case CMD_ACCDST_ID:
		if (perf_ll_smpl(0, 0) == 0) {
			return (B_TRUE);
		}
		break;

	default:
		break;
	}

	return (B_FALSE);
}
