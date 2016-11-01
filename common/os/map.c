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

/* This file contains the routines to process address mapping in running process. */

#include <link.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <numa.h>
#include "../include/util.h"
#include "../include/proc.h"
#include "../include/os/os_util.h"
#include "../include/os/map.h"

int
map_init(void)
{
	pagesize_init();
	return (0);
}

void
map_fini(void)
{
	/* Not supported in Linux. */
}

static unsigned int
attr_bitmap(char *attr_str)
{
	unsigned int bitmap = 0;
	
	if (attr_str[0] == 'r') {
		MAP_R_SET(bitmap);
	}

	if (attr_str[1] == 'w') {
		MAP_W_SET(bitmap);
	}

	if (attr_str[2] == 'x') {
		MAP_X_SET(bitmap);
	}

	if (attr_str[3] == 'p') {
		MAP_P_SET(bitmap);
	} else if (attr_str[3] == 's') {
		MAP_S_SET(bitmap);
	}

	return (bitmap);
}

static int
map_entry_add(map_proc_t *map, uint64_t start_addr, uint64_t end_addr,
	unsigned int attr, char *path)
{
	map_entry_t *entry;

	if (array_alloc((void **)(&map->arr), &map->nentry_cur,
		&map->nentry_max, sizeof (map_entry_t), MAP_ENTRY_NUM) != 0) {
		return (-1);
	}
	
	entry = &(map->arr[map->nentry_cur]);
	entry->start_addr = start_addr;
	entry->end_addr = end_addr;
	entry->attr = attr;
	entry->need_resolve = B_TRUE;
	memset(&entry->numa_map, 0, sizeof (numa_map_t));

	if (strlen(path) > 0) {
		strncpy(entry->desc, path, PATH_MAX);
		entry->desc[PATH_MAX - 1] = 0;
	} else {
		entry->desc[0] = 0;
	}

	map->nentry_cur++;
	return (0);
}

static void
numa_map_fini(map_entry_t *entry)
{
	numa_map_t *numa;

	numa = &entry->numa_map;
	if (numa->arr != NULL) {
		free(numa->arr);
	}
		
	memset(numa, 0, sizeof (numa_map_t));
}

static void
map_free(map_proc_t *map)
{
	int i;

	if (map->arr == NULL) {
		return;
	}
	
	for (i = 0; i < map->nentry_cur; i++) {
		numa_map_fini(&map->arr[i]);
	}

	free(map->arr);
	memset(map, 0, sizeof (map_proc_t));
}

static int
map_read(pid_t pid, map_proc_t *map)
{
	char path[PATH_MAX];
	char line[MAPFILE_LINE_SIZE];
	char addr_str[128], attr_str[128], off_str[128];
	char fd_str[128], inode_str[128], path_str[PATH_MAX];
	char s1[64], s2[64];
	uint64_t start_addr, end_addr;
	unsigned int attr;
	int nargs, nadded = 0, ret = -1;
	FILE *fp;
	
	memset(map, 0, sizeof (map_proc_t));
	snprintf(path, sizeof (path), "/proc/%d/maps", pid);
	if ((fp = fopen(path, "r")) == NULL) {
		return (-1);
	}	
	
	while (1) {
		if (fgets(line, sizeof (line), fp) == NULL) {
			break;
		}

		/* 
		 * e.g. 00400000-00405000 r-xp 00000000 fd:00 678793	/usr/bin/vmstat
		 */
		if ((nargs = sscanf(line, "%127[^ ] %127[^ ] %127[^ ] %127[^ ] %127[^ ] %4095[^\n]",
		    addr_str, attr_str, off_str, fd_str, inode_str, path_str)) < 0) {
		    goto L_EXIT;
		}
				
		/*
		 * split to start_addr and end_addr.
		 * e.g. 00400000-00405000 -> start_addr = 00400000, end_addr = 00405000.
		 */
    	if (sscanf(addr_str, "%63[^-]", s1) <= 0) {
    		goto L_EXIT;
    	}
		
		if (sscanf(addr_str, "%*[^-]-%63s", s2) <= 0) {
    		goto L_EXIT;
		}

		start_addr = strtoull(s1, NULL, 16);
		end_addr = strtoull(s2, NULL, 16);

		/*
		 * Convert to the attribute bitmap
		 */
		attr = attr_bitmap(attr_str);

		/*
		 * Path could be null, need to check here.
		 */	
		if (nargs != 6) {
			path_str[0] = 0;
		}
		
		if (map_entry_add(map, start_addr, end_addr, attr, path_str) != 0) {
			goto L_EXIT;	
		}
		
		nadded++;
	}

	if (nadded > 0) {	
		map->loaded = B_TRUE;
		ret = 0;
	}

L_EXIT:
	fclose(fp);
	if ((ret != 0) && (nadded > 0)) {
		map_free(map);
	}

	return (ret);
}

int
map_proc_load(track_proc_t *proc)
{
	map_proc_t *map = &proc->map;
	map_proc_t new_map;
	map_entry_t *old_entry;
	int i;

	if (!map->loaded) {
		if (map_read(proc->pid, map) != 0) {
			return (-1);
		}

		return (0);
	}

	if (map_read(proc->pid, &new_map) != 0) {
		return (-1);
	}

	for (i = 0; i < new_map.nentry_cur; i++) {
		if ((old_entry = map_entry_find(proc, new_map.arr[i].start_addr, 
			new_map.arr[i].end_addr - new_map.arr[i].start_addr)) == NULL) {
			new_map.arr[i].need_resolve = B_TRUE;
		} else {
			new_map.arr[i].need_resolve = old_entry->need_resolve;
		}
	}
	
	map_free(&proc->map);
	memcpy(&proc->map, &new_map, sizeof (map_proc_t));
	return (0);	
}

int
map_proc_fini(track_proc_t *proc)
{
	map_free(&proc->map);
	return (0);
}

/*
 * The callback function used in bsearch() to compare the buffer address.
 */
static int
entryaddr_cmp(const void *p1, const void *p2)
{
	const uint64_t addr = *(const uint64_t *)p1;
	const map_entry_t *entry = (const map_entry_t *)p2;
		
	if (addr < entry->start_addr) {
		return (-1);
	}

	if (addr >= entry->end_addr) {
		return (1);
	}

	return (0);
}

map_entry_t *
map_entry_find(track_proc_t *proc, uint64_t addr, uint64_t size)
{
	map_entry_t *entry;

	entry = bsearch(&addr, proc->map.arr, proc->map.nentry_cur,
		sizeof (map_entry_t), entryaddr_cmp);

	if (entry != NULL) {
		if ((entry->start_addr == addr) && (entry->end_addr == addr + size)) {
			return (entry);
		}
	}

	return (NULL);
}

static numa_entry_t *
numa_entry_add(numa_map_t *numa_map, uint64_t addr, int nid)
{
	numa_entry_t *entry;

	if (array_alloc((void **)(&numa_map->arr), &numa_map->nentry_cur,
		&numa_map->nentry_max, sizeof (numa_entry_t), MAP_ENTRY_NUM) != 0) {
		return (NULL);
	}

	entry = &(numa_map->arr[numa_map->nentry_cur]);
	entry->start_addr = addr;
	entry->end_addr = addr + g_pagesize;
	entry->nid = nid;
	numa_map->nentry_cur++;
	return (entry);
}

static numa_entry_t *
numa_map_update(numa_map_t *numa_map, void **addr_arr, int *node_arr,
	int addr_num, numa_entry_t *last_entry)
{
	numa_entry_t *entry;
	int i = 0, j;

	if ((entry = last_entry) == NULL) {
		if ((entry = numa_entry_add(numa_map, (uint64_t)(addr_arr[i]), 
			node_arr[i])) == NULL) {
			return (NULL);
		}

		i++;
	}

	for (j = i; j < addr_num; j++) {
		if ((entry->nid == node_arr[j]) &&
			(entry->end_addr == (uint64_t)(addr_arr[j]))) {
			entry->end_addr += g_pagesize;
		} else {
			if ((entry = numa_entry_add(numa_map, (uint64_t)(addr_arr[j]), 
				node_arr[j])) == NULL) {
				return (NULL);
			}		
		}
	}
		
	return (entry);
}

int
map_map2numa(track_proc_t *proc, map_entry_t *map_entry)
{
	void *addr_arr[NUMA_MOVE_NPAGES];
	unsigned int i, npages_total, npages_tomove, npages_moved = 0;
	int node_arr[NUMA_MOVE_NPAGES];
	numa_entry_t *last_entry = NULL;
	
	numa_map_fini(map_entry);
	
	npages_total = (map_entry->end_addr - map_entry->start_addr) / g_pagesize;
	while (npages_moved < npages_total) {
		npages_tomove = MIN(NUMA_MOVE_NPAGES, npages_total - npages_moved);
		for (i = 0; i < npages_tomove; i++) {
			addr_arr[i] = (void *)(map_entry->start_addr + 
				(i + npages_moved) * g_pagesize);
		}

		memset(node_arr, 0, sizeof (node_arr));		
		if (numa_move_pages(proc->pid, npages_tomove, addr_arr, NULL,
			node_arr, 0) != 0) {
			return (-1);
		}

		if ((last_entry = numa_map_update(&map_entry->numa_map, addr_arr,
			node_arr, npages_tomove, last_entry)) == NULL) {
			return (-1);			
		}

		npages_moved += npages_tomove;
	}

	return (0);
}

int
map_addr2nodedst(pid_t pid, void **addr_arr, int *lat_arr, int addr_num,
	map_nodedst_t *nodedst_arr, int nnodes, int *naccess_total)
{
	int *status_arr, i, nid;
	
	if ((status_arr = zalloc(sizeof (int) * addr_num)) == NULL) {
		return (-1);
	}

	if (numa_move_pages(pid, addr_num, addr_arr, NULL, status_arr, 0) != 0) {
		free(status_arr);
		return (-1);
	}

	*naccess_total = 0;
	for (i = 0; i < addr_num; i++) {
		nid = status_arr[i];
		if ((nid >= 0) && (nid < nnodes)) {
			nodedst_arr[nid].naccess++;
			nodedst_arr[nid].total_lat += lat_arr[i];
			*naccess_total += 1;
		}
	}

	free(status_arr);
	return (0);
}
