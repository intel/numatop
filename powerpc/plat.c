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
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "../common/include/os/plat.h"
#include "../common/include/os/os_util.h"
#include "include/types.h"
#include "include/power8.h"
#include "include/power9.h"
#include "include/power10.h"

pfn_plat_profiling_config_t
s_plat_profiling_config[CPU_TYPE_NUM] = {
	NULL,
	power8_profiling_config,
	power9_profiling_config,
	power10_profiling_config
};

pfn_plat_ll_config_t
s_plat_ll_config[CPU_TYPE_NUM] = {
	NULL,
	power8_ll_config,
	power9_ll_config,
	power10_ll_config
};

pfn_plat_offcore_num_t
s_plat_offcore_num[CPU_TYPE_NUM] = {
	NULL,
	power8_offcore_num,
	power9_offcore_num,
	power10_offcore_num
};

#define SPRN_PVR	0x11F
#define PVR_VER(pvr)	(((pvr) >> 16) & 0xFFFF)
#define PVR_REV(pvr)	(((pvr) >>  0) & 0xFFFF)

int
plat_detect(void)
{
	int ret = -1;
	int pvr = 0;

	asm __volatile__
	    ("mfspr %0,%1" : "=r"(pvr) : "i"(SPRN_PVR));

	switch(PVR_VER(pvr)) {
	case 0x4b:
	case 0x4c:
	case 0x4d:
		s_cpu_type = CPU_POWER8;
		ret = 0;
		break;
	case 0x4e:
		/* No support for Power9 DD1. */
		if (PVR_REV(pvr) == 0x100)
			break;

		s_cpu_type = CPU_POWER9;
		ret = 0;
		break;
	case 0x80:
		s_cpu_type = CPU_POWER10;
		ret = 0;
		break;
	}

	return ret;
}
