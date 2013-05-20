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

#ifndef _NUMATOP_MAP_H
#define	_NUMATOP_MAP_H

#include <sys/types.h>
#include <stdio.h>
#include "../types.h"
#include "sym.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAP_R(attr) \
	(((attr) >> 3) & 0x1)

#define MAP_W(attr) \
	(((attr) >> 2) & 0x1)

#define MAP_X(attr) \
	(((attr) >> 1) & 0x1)

#define MAP_P(attr) \
	(((attr) & 0x1) == 0)

#define MAP_S(attr) \
	(((attr) & 0x1) == 1)

#define MAP_R_SET(attr) \
	((attr) |= (1 << 3))

#define MAP_W_SET(attr) \
	((attr) |= (1 << 2))

#define MAP_X_SET(attr) \
	((attr) |= (1 << 1))

#define MAP_P_SET(attr) \
	((attr) &= (unsigned int)(~1))

#define MAP_S_SET(attr) \
	((attr) |= 1)

#define MAPFILE_LINE_SIZE (PATH_MAX + 1024)
#define MAP_ENTRY_NUM	64

typedef struct _numa_entry {
	uint64_t start_addr;
	uint64_t end_addr;
	int nid;
} numa_entry_t;

typedef struct _numa_map {
	numa_entry_t *arr;
	int nentry_cur;
	int nentry_max;
} numa_map_t;

typedef struct _map_entry {
	uint64_t start_addr;
	uint64_t end_addr;
	unsigned int attr;
	boolean_t need_resolve;
	numa_map_t numa_map;
	char desc[PATH_MAX];
} map_entry_t;

typedef struct _map_proc {
	map_entry_t *arr;
	int nentry_cur;
	int nentry_max;
	boolean_t loaded;
} map_proc_t;

typedef struct _map_nodedst {
	int naccess;
	unsigned int total_lat;
} map_nodedst_t;

#define NUMA_MOVE_NPAGES	1024

struct _track_proc;

int map_init(void);
void map_fini(void);
int map_proc_load(struct _track_proc *);
int map_proc_fini(struct _track_proc *);
map_entry_t* map_entry_find(struct _track_proc *, uint64_t, uint64_t);
int map_map2numa(struct _track_proc *, map_entry_t *);
int map_addr2nodedst(pid_t pid, void **, int *, int, map_nodedst_t *,
	int, int *);

#ifdef __cplusplus
}
#endif

#endif /* _NUMATOP_SYM_H */

