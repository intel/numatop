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

#ifndef _NUMATOP_SYM_H
#define	_NUMATOP_SYM_H

#include <sys/types.h>
#include <stdio.h>
#include <elf.h>
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This symbol type is the same as STT_FUNC except that it always 
 * points to a function or piece of executable code which takes no 
 * arguments and which returns a function pointer.
 */
#ifndef STT_IFUNC
#define STT_IFUNC	10
#endif

#define SYM_ITEM_NUM		1024
#define	SYM_NAME_SIZE		32
#define SYM_LIB_NUM			16
#define SYM_CLASS_NUM		2
#define ELF32_LOAD_ADDR		0x08048000
#define ELF64_LOAD_ADDR		0x400000
#define INVALID_LOADADDR	(uint64_t)(-1)

typedef enum {
	SYM_CLASS_INVALID = -1,
	SYM_CLASS_ELF32 = 0,
	SYM_CLASS_ELF64
} sym_class_t;

typedef enum {
	SYM_TYPE_FUNC = 1,
	SYM_TYPE_OBJECT
} sym_type_t;

typedef struct _sym_item {
	char name[SYM_NAME_SIZE];
	sym_type_t type;
	unsigned int index;
	uint64_t off;
	uint64_t size;
} sym_item_t;

typedef struct _sym_binary {
	sym_item_t *items;
	int nitem_cur;
	int nitem_max;
	FILE *fp;
	char path[PATH_MAX];
} sym_binary_t;

typedef struct _sym_lib {
	sym_binary_t binary;
	struct _sym_lib *next;
} sym_lib_t;

typedef struct _sym_libref {
	sym_lib_t **libs;
	uint64_t *lib_loadaddr;
	int nlib_cur;
	int nlib_max;
} sym_libref_t;

typedef struct _sym {
	sym_binary_t image;
	uint64_t image_loadaddr;
	sym_libref_t libref;
	boolean_t loaded;
} sym_t;

typedef struct _sym_ops {
	int (*pfn_binary_read)(sym_binary_t *, sym_type_t);
} sym_ops_t;

typedef struct _sym_callentry {
	uint64_t addr;
	uint64_t size;
	char name[SYM_NAME_SIZE];
} sym_callentry_t;

typedef struct _sym_callchain {
	sym_callentry_t *entry_arr;
	int nentry;
	int naccess;
	struct _sym_callchain *prev;
	struct _sym_callchain *next;
} sym_callchain_t;

typedef struct _sym_chainlist {
	sym_callchain_t *head;
	sym_callchain_t *tail;
	int num;
} sym_chainlist_t;

#define IP_HIT(ip, addr, size) \
	(((ip) >= (addr)) && ((ip) < (addr) + (size)))

struct _track_proc;

void sym_init(void);
void sym_fini(void);
int sym_load(struct _track_proc *, sym_type_t);
void sym_free(sym_t *);
int sym_callchain_add(sym_t *, uint64_t *, int, sym_chainlist_t *);
void sym_callchain_resort(sym_chainlist_t *);
sym_callchain_t* sym_callchain_detach(sym_chainlist_t *);
void sym_callchain_free(sym_callchain_t *);
void sym_chainlist_free(sym_chainlist_t *);
int sym_chainlist_nentry(sym_chainlist_t *, int *);

#ifdef __cplusplus
}
#endif

#endif /* _NUMATOP_SYM_H */
