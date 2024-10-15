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

#include <string.h>
#include "../common/include/os/plat.h"
#include "include/types.h"
#include "include/util.h"
#include "include/nhm.h"
#include "include/wsm.h"
#include "include/snb.h"
#include "include/bdw.h"
#include "include/skl.h"
#include "include/srf.h"
#include "include/zen.h"

pfn_plat_profiling_config_t
s_plat_profiling_config[CPU_TYPE_NUM] = {
	NULL,
	wsmex_profiling_config,
	snbep_profiling_config,
	nhmex_profiling_config,
	nhmep_profiling_config,
	wsmep_profiling_config,
	snbep_profiling_config,
	snbep_profiling_config,
	bdw_profiling_config,
	skl_profiling_config,
	icx_profiling_config,
	spr_profiling_config,
	spr_profiling_config,	/* EMR */
	spr_profiling_config,	/* GNR */
	srf_profiling_config,
	zen_profiling_config,
	zen3_profiling_config,
	zen4_profiling_config
};

pfn_plat_ll_config_t
s_plat_ll_config[CPU_TYPE_NUM] = {
	NULL,
	wsmex_ll_config,
	snbep_ll_config,
	nhmex_ll_config,
	nhmep_ll_config,
	wsmep_ll_config,
	snbep_ll_config,
	snbep_ll_config,
	bdw_ll_config,
	skl_ll_config,
	icx_ll_config,
	spr_ll_config,
	spr_ll_config,		/* EMR */
	spr_ll_config,		/* GNR */
	srf_ll_config,
	zen_ll_config,
	zen_ll_config,
	zen_ll_config
};

pfn_plat_offcore_num_t
s_plat_offcore_num[CPU_TYPE_NUM] = {
	NULL,
	wsm_offcore_num,
	snb_offcore_num,
	nhm_offcore_num,
	nhm_offcore_num,
	wsm_offcore_num,
	snb_offcore_num,
	snb_offcore_num,
	bdw_offcore_num,
	skl_offcore_num,
	icx_offcore_num,
	spr_offcore_num,
	spr_offcore_num,	/* EMR */
	spr_offcore_num,	/* GNR */
	srf_offcore_num,
	zen_offcore_num,
	zen_offcore_num,
	zen_offcore_num
};

/* ARGSUSED */
static void
cpuid(unsigned int *eax, unsigned int *ebx, unsigned int *ecx,
	unsigned int *edx)
{
#if __x86_64
	__asm volatile(
	    "cpuid\n\t"
	    :"=a" (*eax),
	    "=b" (*ebx),
	    "=c" (*ecx),
	    "=d" (*edx)
	    :"a" (*eax));
#else
	__asm volatile(
	    "push %%ebx\n\t"
	    "cpuid\n\t"
	    "mov %%ebx, (%4)\n\t"
	    "pop %%ebx"
	    :"=a" (*eax),
	    "=c" (*ecx),
	    "=d" (*edx)
	    :"0" (*eax),
	    "S" (ebx)
	    :"memory");
#endif
}

static cpu_type_t
cpu_type_get(void)
{
	unsigned int eax, ebx, ecx, edx;
	int family, model;
	cpu_type_t type = CPU_UNSUP;
	char vendor[16];

	eax = 0;
	cpuid(&eax, &ebx, &ecx, &edx);

	(void) strncpy(&vendor[0], (char *)(&ebx), 4);
	(void) strncpy(&vendor[4], (char *)(&ecx), 4);
	(void) strncpy(&vendor[8], (char *)(&edx), 4);
	vendor[12] = 0;

	if (strncmp(vendor, "Genu" "ntel" "ineI", 12) != 0 &&
	    strncmp(vendor, "Auth" "cAMD" "enti", 12) != 0) {
		return (CPU_UNSUP);
	}

	eax = 1;
	cpuid(&eax, &ebx, &ecx, &edx);

	family = CPU_FAMILY(eax);
	model = CPU_MODEL(eax);

	/* Extended Model ID is considered only when Family ID is either 6 or 15 */
	if (family == 6 || family == 15)
		model += CPU_EXT_MODEL(eax) << 4;

	/* Extended Family ID is considered only when Family ID is 15 */
	if (family == 15)
		family += CPU_EXT_FAMILY(eax);

	if (family == 6) {
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
		case 62:
			type = CPU_IVB_EX;
			break;
		case 63:
			type = CPU_HSX;
			break;
		case 79:
			type = CPU_BDX;
			break;
		case 85:
			type = CPU_SKX;
			break;
		case 106:
			type = CPU_ICX;
			break;
		case 143:
                        type = CPU_SPR;
			break;
		case 207:
                        type = CPU_EMR;
			break;
		case 173:
			type = CPU_GNR;
			break;
		case 175:
			type = CPU_SRF;
			break;
		}
	} else if (family == 23) {	/* Family 17h */
		type = CPU_ZEN;
	} else if (family == 25) {	/* Family 19h */
		if ((model >= 0x00 && model <= 0x0f) ||
		    (model >= 0x20 && model <= 0x2f) ||
		    (model >= 0x40 && model <= 0x5f)) {
			type = CPU_ZEN3;
		} else {
			type = CPU_ZEN4;
		}
	} else if (family >= 26) {	/* Family 1Ah and later */
		type = CPU_ZEN4;
	}

	return (type);
}

/*
 * NumaTOP needs some special performance counters,
 * It can only run on WSM-EX/SNB-EP platforms now.
 */
int
plat_detect(void)
{
	int ret = -1;
	cpu_type_t cpu_type;

	if ((cpu_type = cpu_type_get()) == CPU_UNSUP) {
		return (-1);
	}

	switch (cpu_type) {
	case CPU_WSM_EX:
		/* fall through */
	case CPU_SNB_EP:
		/* fall through */
	case CPU_NHM_EX:
		/* fall through */
	case CPU_NHM_EP:
		/* fall through */
	case CPU_WSM_EP:
		/* fall through */
	case CPU_IVB_EX:
		/* fall through */
	case CPU_HSX:
		/* fall through */
	case CPU_BDX:
		/* fall through */
	case CPU_SKX:
	case CPU_ICX:
	case CPU_SPR:
	case CPU_EMR:
	case CPU_GNR:
	case CPU_SRF:
	case CPU_ZEN:
	case CPU_ZEN3:
	case CPU_ZEN4:
		ret = 0;
		s_cpu_type = cpu_type;
		break;
	default:
		break;
	}

	return (ret);
}
