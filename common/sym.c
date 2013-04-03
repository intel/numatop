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

/* This file contains the routines to process symbols in running process. */

#include <link.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include "include/util.h"
#include "include/sym.h"
#include "include/map.h"
#include "include/proc.h"

static sym_lib_t *s_first_lib;

static int elf32_binary_read(sym_binary_t *, sym_type_t);
static int elf64_binary_read(sym_binary_t *, sym_type_t);

static sym_ops_t s_sym_ops[SYM_CLASS_NUM] = {
	{ elf32_binary_read },
	{ elf64_binary_read }
};

void
sym_init(void)
{
	s_first_lib = NULL;
}

static void
sym_binary_fini(sym_binary_t *binary)
{
	if (binary->items != NULL) {
		free(binary->items);
	}
	
	if (binary->fp != NULL) {
		fclose(binary->fp);
	}
	
	memset(binary, 0, sizeof (sym_binary_t));
}

void
sym_fini(void)
{
	sym_lib_t *p1, *p2 = s_first_lib;
	
	while (p2 != NULL) {
		p1 = p2->next;
		sym_binary_fini(&p2->binary);
		free(p2);
		p2 = p1;
	}

	s_first_lib = NULL;
}

static boolean_t
magic_match(unsigned char *ident)
{
	if ((ident[EI_MAG0] == ELFMAG0) &&
		(ident[EI_MAG1] == ELFMAG1) &&
		(ident[EI_MAG2] == ELFMAG2) &&
		(ident[EI_MAG3] == ELFMAG3)) {
		return (B_TRUE);
	}

	return (B_FALSE);
}

static sym_class_t
elf_class(FILE *fp)
{
	unsigned char e_ident[EI_NIDENT];
	
	if (fseek(fp, 0, SEEK_SET) != 0) {
		return (SYM_CLASS_INVALID);
	}

	if (fread(e_ident, EI_NIDENT, 1, fp) != 1) {
		return (SYM_CLASS_INVALID);
	}

	if (!magic_match(e_ident)) {
		return (SYM_CLASS_INVALID);
	}

	switch (e_ident[EI_CLASS]) {
	case ELFCLASS32:		
		return (SYM_CLASS_ELF32);

	case ELFCLASS64:
		return (SYM_CLASS_ELF64);
		
	default:
		break;
	}

	return (SYM_CLASS_INVALID);
}

static uint64_t
elf32_seg_loadaddr(FILE *fp, uint64_t phoff, int phnum, unsigned int flags)
{
	Elf32_Phdr phdr;
	uint64_t load_addr = INVALID_LOADADDR;
	int i;

	if (fseek(fp, phoff, SEEK_SET) != 0) {
		goto L_EXIT;
	}

	for (i = 0; i < phnum; i++) {
		if (fread(&phdr, sizeof (Elf32_Phdr), 1, fp) != 1) {
			goto L_EXIT;
		}
		
		if ((phdr.p_type == PT_LOAD) &&
			((phdr.p_flags & flags) != 0)) {
			break;
		}
	}
	
	if (i == phnum) {
		goto L_EXIT;
	}
	
	load_addr = phdr.p_vaddr;

L_EXIT:
	return (load_addr);
}

static uint64_t
elf64_seg_loadaddr(FILE *fp, uint64_t phoff, int phnum, unsigned int flags)
{
	Elf64_Phdr phdr;
	uint64_t load_addr = INVALID_LOADADDR;
	int i;

	if (fseek(fp, phoff, SEEK_SET) != 0) {
		goto L_EXIT;
	}

	for (i = 0; i < phnum; i++) {
		if (fread(&phdr, sizeof (Elf64_Phdr), 1, fp) != 1) {
			goto L_EXIT;
		}
		
		if ((phdr.p_type == PT_LOAD) &&
			((phdr.p_flags & flags) != 0)) {
			break;
		}
	}
	
	if (i == phnum) {
		goto L_EXIT;
	}
	
	load_addr = phdr.p_vaddr;

L_EXIT:
	return (load_addr);
}

static int
sym_item_add(sym_binary_t *binary, char *sym_name, uint64_t sym_addr,
	uint64_t sym_size, sym_type_t sym_type, uint64_t load_addr)
{
	sym_item_t *item;

	if (array_alloc((void **)&binary->items, &binary->nitem_cur,
		&binary->nitem_max, sizeof (sym_item_t), SYM_ITEM_NUM) != 0) {
		return (-1);
	}

	item = &binary->items[binary->nitem_cur];
	strncpy(item->name, sym_name, SYM_NAME_SIZE);
	item->name[SYM_NAME_SIZE - 1] = 0;
	item->type = sym_type;
	item->off = sym_addr - load_addr;
	item->size = sym_size;
	binary->nitem_cur++;
	return (0);
}

static int
elf32_symtab_copyout(sym_binary_t *binary, uint64_t symtab_off,
	int symtab_num, uint64_t strtab_off, uint64_t strtab_size,
	uint64_t load_addr, sym_type_t sym_type)
{
	Elf32_Sym sym;
	char *strtab;
	int i, ret = -1;

	if (symtab_num == 0) {
		return (0);
	}
	
	if ((strtab = zalloc(strtab_size + 1)) == NULL) {
		return (-1);
	}

	if (fseek(binary->fp, strtab_off, SEEK_SET) != 0) {
		goto L_EXIT;
	}

	if (fread(strtab, strtab_size, 1, binary->fp) != 1) {
		goto L_EXIT;
	}

	if (fseek(binary->fp, symtab_off, SEEK_SET) != 0) {
		goto L_EXIT;
	}

	for (i = 0; i < symtab_num; i++) {
		if (fread(&sym, sizeof (Elf32_Sym), 1, binary->fp) != 1) {
			goto L_EXIT;
		}

		if (sym.st_size == 0) {
			continue;	
		}

		switch (sym_type) {
		case SYM_TYPE_FUNC:
			if ((ELF32_ST_TYPE(sym.st_info) == STT_FUNC) ||
				(ELF32_ST_TYPE(sym.st_info) == STT_IFUNC)) {

				if (sym_item_add(binary, strtab + sym.st_name, sym.st_value,
					sym.st_size, sym_type, load_addr) != 0) {
					goto L_EXIT;
				}
			}
			break;

		default:
			break;
		}
	}

	ret = 0;	

L_EXIT:
	free(strtab);
	return (ret);
}

static int
elf64_symtab_copyout(sym_binary_t *binary, uint64_t symtab_off,
	int symtab_num, uint64_t strtab_off, uint64_t strtab_size,
	uint64_t load_addr, sym_type_t sym_type)
{
	Elf64_Sym sym;
	char *strtab;
	int i, ret = -1;

	if (symtab_num == 0) {
		return (0);
	}
	
	if ((strtab = zalloc(strtab_size + 1)) == NULL) {
		return (-1);
	}

	if (fseek(binary->fp, strtab_off, SEEK_SET) != 0) {
		goto L_EXIT;
	}

	if (fread(strtab, strtab_size, 1, binary->fp) != 1) {
		goto L_EXIT;
	}

	if (fseek(binary->fp, symtab_off, SEEK_SET) != 0) {
		goto L_EXIT;
	}

	for (i = 0; i < symtab_num; i++) {
		if (fread(&sym, sizeof (Elf64_Sym), 1, binary->fp) != 1) {
			goto L_EXIT;
		}

		if (sym.st_size == 0) {
			continue;	
		}

		switch (sym_type) {
		case SYM_TYPE_FUNC:
			if ((ELF64_ST_TYPE(sym.st_info) == STT_FUNC) ||
				(ELF64_ST_TYPE(sym.st_info) == STT_IFUNC)) {

				if (sym_item_add(binary, strtab + sym.st_name, sym.st_value,
					sym.st_size, sym_type, load_addr) != 0) {
					goto L_EXIT;
				}
			}
			break;

		default:
			break;
		}
	}

	ret = 0;	

L_EXIT:
	free(strtab);
	return (ret);
}

static int
sym_cmp(const void *a, const void *b)
{
	sym_item_t *s1 = (sym_item_t *)a;
	sym_item_t *s2 = (sym_item_t *)b;

	if (s1->off < s2->off) {
		return (-1);
	}
	
	if (s1->off > s2->off) {
		return (1);
	}

	return (0);
}

static void
item_sort(sym_binary_t *binary)
{
	int i;

	qsort(binary->items, binary->nitem_cur, sizeof (sym_item_t), sym_cmp);
	for (i = 0; i < binary->nitem_cur; i++) {
		binary->items[i].index = i;
	}	
}

static int
elf32_binary_read(sym_binary_t *binary, sym_type_t sym_type)
{
	Elf32_Ehdr ehdr;
	Elf32_Shdr shdr;
	char *shstrtab;
	uint64_t load_addr;
	uint64_t symtab_off = 0, strtab_off = 0, strtab_size = 0;
	int symtab_num = 0, i, ret = -1;

	if (fseek(binary->fp, 0, SEEK_SET) != 0) {
		return (-1);
	}

	if (fread(&ehdr, sizeof (Elf32_Ehdr), 1, binary->fp) != 1) {
		return (-1);
	}

	if (ehdr.e_shentsize != sizeof (Elf32_Shdr)) {
		debug_print(NULL, 2, "elf32_binary_read: ehdr.e_shentsize != %d\n",
			sizeof (Elf32_Shdr));
		return (-1);
	}

	/*
	 * Get the load address of "x" segment.
	 */
	if ((load_addr = elf32_seg_loadaddr(binary->fp, ehdr.e_phoff,
		ehdr.e_phnum, PF_X)) == INVALID_LOADADDR) {
		return (-1);
	}

	/*
	 * Read the "shstrtab" to a buffer.
	 */
	if (fseek(binary->fp, ehdr.e_shoff + ehdr.e_shstrndx * sizeof (Elf32_Shdr),
		SEEK_SET) != 0) {
		return (-1);
	}
	
	if (fread(&shdr, sizeof (Elf32_Shdr), 1, binary->fp) != 1) {
		return (-1);
	}	

	if ((shstrtab = zalloc(shdr.sh_size)) == NULL) {
		return (-1);
	}
	
	if (fseek(binary->fp, shdr.sh_offset, SEEK_SET) != 0) {
		goto L_EXIT;
	}

	if (fread(shstrtab, shdr.sh_size, 1, binary->fp) != 1) {
		goto L_EXIT;
	}	

	/*
	 * Move to the start of sections.
	 */
	if (fseek(binary->fp, ehdr.e_shoff, SEEK_SET) != 0) {
		goto L_EXIT;
	}

	/*
	 * Walk on each section.
	 */
	for (i = 0; i < ehdr.e_shnum; i++) {
		if (fread(&shdr, sizeof (Elf32_Shdr), 1, binary->fp) != 1) {
			goto L_EXIT;
		}

		if ((strcmp(shstrtab + shdr.sh_name, ".symtab") == 0) ||
			(strcmp(shstrtab + shdr.sh_name, ".dynsym") == 0)) {
			symtab_off = shdr.sh_offset;
			symtab_num = (int)(shdr.sh_size / shdr.sh_entsize);
		}
	
		if ((strcmp(shstrtab + shdr.sh_name, ".strtab") == 0) ||
			(strcmp(shstrtab + shdr.sh_name, ".dynstr") == 0)) {
			strtab_off = shdr.sh_offset;
			strtab_size = shdr.sh_size;
		}
	}
	
	if (elf32_symtab_copyout(binary, symtab_off, symtab_num, strtab_off,
		strtab_size, load_addr, sym_type) != 0) {
		goto L_EXIT;
	}

	item_sort(binary);
	ret = 0;

L_EXIT:
	free(shstrtab);	
	return (ret);
}

static int
elf64_binary_read(sym_binary_t *binary, unsigned int sym_type)
{
	Elf64_Ehdr ehdr;
	Elf64_Shdr shdr;
	char *shstrtab;
	uint64_t load_addr;
	uint64_t symtab_off = 0, strtab_off = 0, strtab_size = 0;
	int symtab_num = 0, i, ret = -1;

	if (fseek(binary->fp, 0, SEEK_SET) != 0) {
		return (-1);
	}

	if (fread(&ehdr, sizeof (Elf64_Ehdr), 1, binary->fp) != 1) {
		return (-1);
	}

	if (ehdr.e_shentsize != sizeof (Elf64_Shdr)) {
		debug_print(NULL, 2, "elf64_binary_read: ehdr.e_shentsize != %d\n",
			sizeof (Elf64_Shdr));
		return (-1);
	}

	/*
	 * Get the load address of "x" segment.
	 */
	if ((load_addr = elf64_seg_loadaddr(binary->fp, ehdr.e_phoff,
		ehdr.e_phnum, PF_X)) == INVALID_LOADADDR) {
		return (-1);
	}

	/*
	 * Read the "shstrtab" to a buffer.
	 */
	if (fseek(binary->fp, ehdr.e_shoff + ehdr.e_shstrndx * sizeof (Elf64_Shdr),
		SEEK_SET) != 0) {
		return (-1);
	}
	
	if (fread(&shdr, sizeof (Elf64_Shdr), 1, binary->fp) != 1) {
		return (-1);
	}	

	if ((shstrtab = zalloc(shdr.sh_size)) == NULL) {
		return (-1);
	}
	
	if (fseek(binary->fp, shdr.sh_offset, SEEK_SET) != 0) {
		goto L_EXIT;
	}

	if (fread(shstrtab, shdr.sh_size, 1, binary->fp) != 1) {
		goto L_EXIT;
	}	

	/*
	 * Move to the start of sections.
	 */
	if (fseek(binary->fp, ehdr.e_shoff, SEEK_SET) != 0) {
		goto L_EXIT;
	}

	/*
	 * Walk on each section.
	 */
	for (i = 0; i < ehdr.e_shnum; i++) {
		if (fread(&shdr, sizeof (Elf64_Shdr), 1, binary->fp) != 1) {
			goto L_EXIT;
		}

		if ((strcmp(shstrtab + shdr.sh_name, ".symtab") == 0) ||
			(strcmp(shstrtab + shdr.sh_name, ".dynsym") == 0)) {
			symtab_off = shdr.sh_offset;
			symtab_num = (int)(shdr.sh_size / shdr.sh_entsize);
		}
	
		if ((strcmp(shstrtab + shdr.sh_name, ".strtab") == 0) ||
			(strcmp(shstrtab + shdr.sh_name, ".dynstr") == 0)) {
			strtab_off = shdr.sh_offset;
			strtab_size = shdr.sh_size;
		}
	}
	
	if (elf64_symtab_copyout(binary, symtab_off, symtab_num, strtab_off,
		strtab_size, load_addr, sym_type) != 0) {
		goto L_EXIT;
	}

	item_sort(binary);
	ret = 0;

L_EXIT:
	free(shstrtab);	
	return (ret);	
}

static int
binary_sym_read(sym_binary_t *binary, sym_type_t sym_type)
{
	sym_class_t cls;
	int ret = -1;

	if ((binary->fp = fopen(binary->path, "r")) == NULL) {		
		return (-1);
	}
		
	if ((cls = elf_class(binary->fp)) == SYM_CLASS_INVALID) {
		goto L_EXIT;
	}

	if ((s_sym_ops[cls].pfn_binary_read)(binary, sym_type) != 0) {
		goto L_EXIT;
	}

	ret = 0;

L_EXIT:
	fclose(binary->fp);
	binary->fp = NULL;
	return (ret);	
}

static int
image_sym_read(sym_t *sym, map_entry_t *map, sym_type_t sym_type)
{
	sym_binary_t *binary = &sym->image;
	
	strncpy(binary->path, map->desc, PATH_MAX);
	binary->path[PATH_MAX - 1] = 0;

	if (binary_sym_read(binary, sym_type) != 0) {
		return (-1);		
	}

	if (MAP_X(map->attr)) {
		sym->image_loadaddr = map->start_addr;
	}

	return (0);
}

static sym_lib_t *
lib_find(char *path)
{
	sym_lib_t *p = s_first_lib;
	
	while (p != NULL) {
		if (strcmp(p->binary.path, path) == 0) {
			return (p);
		}

		p = p->next;
	}

	return (NULL);
}

static sym_lib_t *
lib_add(char *path, sym_type_t sym_type)
{
	sym_lib_t *lib, *p;
	sym_binary_t *binary;
	
	if ((lib = zalloc(sizeof (sym_lib_t))) == NULL) {
		return (NULL);
	}

	binary = &lib->binary;
	strncpy(binary->path, path, PATH_MAX);
	binary->path[PATH_MAX - 1] = 0;

	if (binary_sym_read(binary, sym_type) != 0) {
		free(lib);
		return (NULL);		
	}
	
	p = s_first_lib;
	s_first_lib = lib;
	lib->next = p;
	return (lib);
}

static int
libref_add(sym_libref_t *ref, sym_lib_t *lib, map_entry_t *map)
{
	int i, j;

	i = ref->nlib_cur;
	j = ref->nlib_max;

	if (array_alloc((void **)(&ref->libs), &ref->nlib_cur, &ref->nlib_max,
		sizeof (uint64_t), SYM_LIB_NUM) != 0) {
		return (-1);
	}

	if (array_alloc((void **)(&ref->lib_loadaddr), &i, &j,
		sizeof (uint64_t), SYM_LIB_NUM) != 0) {
		return (-1);
	}

	i = ref->nlib_cur;
	ref->libs[i] = lib;
	ref->lib_loadaddr[i] = map->start_addr;
	ref->nlib_cur++;
	return (0);
}

static void
libref_free(sym_libref_t *ref)
{
	if (ref->libs != NULL) {
		free(ref->libs);
	}

	if (ref->lib_loadaddr != NULL) {
		free(ref->lib_loadaddr);
	}
}

static int
lib_sym_read(sym_t *sym, map_entry_t *map, sym_type_t sym_type)
{
	sym_lib_t *lib;

	if ((lib = lib_find(map->desc)) == NULL) {
		if ((lib = lib_add(map->desc, sym_type)) == NULL) {
			return (-1);
		}
	}

	if (libref_add(&sym->libref, lib, map) != 0) {
		return (-1);
	}

	return (0);
}

int
sym_load(track_proc_t *proc, sym_type_t sym_type)
{
	map_t *map;
	map_entry_t *entry;
	int i;

	if (map_load(proc) != 0) {
		return (-1);
	}

	map = &proc->map;
	for (i = 0; i < map->nentry_cur; i++) {
		entry = &map->arr[i];
		if ((entry->need_resolve) && (MAP_X(entry->attr)) &&
			(sym_type == SYM_TYPE_FUNC)) {

			if (strstr(entry->desc, ".so") != NULL) {
				lib_sym_read(&proc->sym, entry, sym_type);
			} else {
				image_sym_read(&proc->sym, entry, sym_type);
			}

			entry->need_resolve = B_FALSE;
		}
	}
	
	proc->sym.loaded = B_TRUE;
	return (0);
}

void
sym_free(sym_t *sym)
{
	if (sym->loaded) {
		sym_binary_fini(&sym->image);
		libref_free(&sym->libref);
		sym->loaded = B_FALSE;
	}
}

static int
off_cmp(const void *a, const void *b)
{
	uint64_t off = *(uint64_t *)a;
	sym_item_t *item = (sym_item_t *)b;
	
	if (off >= item->off + item->size) {
		return (1);
	}
	
	if (off < item->off) {
		return (-1);
	}

	return (0);
}

static sym_item_t *
resolve(sym_binary_t *binary, uint64_t off, int *num)
{
	sym_item_t *item, *arr;
	int i, j, start;

	if ((item = bsearch(&off, (void *)(binary->items), binary->nitem_cur,
		sizeof (sym_item_t), off_cmp)) == NULL) {
		return (NULL);
	}

	/*
	 * Multiple symbols could be mapped to same address.
	 */
	*num = 1;
	start = item->index;
	i = item->index - 1;
	while (i >= 0) {
		if (binary->items[i].off == off) {
			(*num)++;
			start = i;
			i--;
		} else {
			break;	
		}
	}

	i = item->index + 1;	
	while (i < binary->nitem_cur) {
		if (binary->items[i].off == off) {
			(*num)++;	
			i++;
		} else {
			break;	
		}
	}

	if ((arr = zalloc(sizeof (sym_item_t) * (*num))) == NULL) {
		return (NULL);
	}

	j = 0;
	for (i = start; i < start + *num; i++) {
		arr[j] = binary->items[i];
		j++;
	}

	return (arr);
}

static int
sym_resolve(sym_t *sym, uint64_t addr, sym_item_t **item_arr,
	int *num, uint64_t *base_addr)
{
	sym_libref_t *libref;
	sym_binary_t *binary;
	int i;

	if (!sym->loaded) {
		return (-1);
	}

	if ((*item_arr = resolve(&sym->image,
		addr - sym->image_loadaddr, num)) != NULL) {
		*base_addr = sym->image_loadaddr;
		return (0);
	}

	libref = &sym->libref;
	for (i = 0; i < libref->nlib_cur; i++) {
		binary = &((libref->libs[i])->binary);

		if ((*item_arr = resolve(binary,
			addr - libref->lib_loadaddr[i], num)) != NULL) {
			*base_addr = libref->lib_loadaddr[i];
			return (0);
		}
	}
	
	return (-1);
}

static sym_item_t *
resolve_unique(sym_t *sym, uint64_t addr, sym_item_t **arr, uint64_t *base_addr)
{
	sym_item_t *item_arr, *item;
	int num, i;

	if (sym_resolve(sym, addr, &item_arr, &num, base_addr) != 0) {
		*arr = NULL;
		return (NULL);
	}

	ASSERT(num > 0);
	for (i = 0; i < num; i++) {
		item = &item_arr[i];
		if (item->name[0] != '_') {
			*arr = item_arr;
			return (item);
		}
	}

	*arr = item_arr;
	return (item);
}

static boolean_t
ips_exist(sym_callchain_t *chain, uint64_t *ips, int ips_num)
{
	int i;

	if (ips_num > chain->nentry) {
		return (B_FALSE);
	}

	for (i = 0; i < ips_num; i++) {
		if (!IP_HIT(ips[i], chain->entry_arr[i].addr,
			chain->entry_arr[i].size)) {
			return (B_FALSE);
		}
	}

	return (B_TRUE);
}

static sym_callchain_t *
chain_find(sym_chainlist_t *list, uint64_t *ips, int ips_num)
{
	sym_callchain_t *chain;
	
	chain = list->head;
	while (chain != NULL) {
		if (ips_exist(chain, ips, ips_num)) {
			return (chain);
		}

		chain = chain->next;
	}
	
	return (NULL);
}

static sym_callchain_t *
chain_alloc(sym_t *sym, uint64_t *ips, int ips_num)
{
	sym_callentry_t *entry_arr, *entry;
	sym_item_t *item_arr, *item;
	sym_callchain_t *chain;
	uint64_t base_addr;
	int i;

	if ((entry_arr = zalloc(ips_num * sizeof (sym_callentry_t))) == NULL) {
		return (NULL);
	}
	
	for (i = 0; i < ips_num; i++) {
		entry = &entry_arr[i];
		
		if ((item = resolve_unique(sym, ips[i], &item_arr,
			&base_addr)) == NULL) {

			/*
			 * Can't resolve the symbol, just record the address.
			 */
			entry->addr = ips[i];
			entry->size = sizeof (ips[i]);
			snprintf(entry->name, SYM_NAME_SIZE, "0x%llx", (long long)ips[i]);
		} else {
			/*
			 * Got the symbol.
			 */
			entry->addr = base_addr + item->off;
			entry->size = item->size;
			strncpy(entry->name, item->name, SYM_NAME_SIZE);
			entry->name[SYM_NAME_SIZE - 1] = 0;
			free(item_arr);
		}
	}

	if ((chain = zalloc(sizeof (sym_callchain_t))) == NULL) {
		free(entry_arr);
		return (NULL);
	}
	
	chain->entry_arr = entry_arr;
	chain->nentry = ips_num;
	return (chain);
}

static void
chainlist_attach_tail(sym_chainlist_t *list, sym_callchain_t *chain)
{
	sym_callchain_t *tail = list->tail;

	chain->next = NULL;
	chain->prev = tail;

	if (tail != NULL) {
		tail->next = chain;
	} else {
		list->head = chain;			
	}

	list->tail = chain;	
	list->num++;
}

static void
chainlist_detach(sym_chainlist_t *list, sym_callchain_t *chain)
{
	sym_callchain_t *prev, *next;
	
	prev = chain->prev;
	next = chain->next;
	
	if (prev != NULL) {
		prev->next = next;	
	} else {
		list->head = next;
	}
	
	if (next != NULL) {
		next->prev = prev;
	} else {
		list->tail = prev;
	}

	chain->prev = NULL;
	chain->next = NULL;
	list->num--;	
}

int
sym_callchain_add(sym_t *sym, uint64_t *ips, int ips_num,
	sym_chainlist_t *list)
{
	sym_callchain_t *chain;

	if ((chain = chain_find(list, ips, ips_num)) != NULL) {
		chain->naccess++;
		return (0);	
	}
	
	if ((chain = chain_alloc(sym, ips, ips_num)) == NULL) {
		return (-1);
	}

	chainlist_attach_tail(list, chain);
	return (0);
}

static sym_callchain_t *
max_access_chain(sym_chainlist_t *list)
{
	sym_callchain_t *p = list->head, *found = NULL;
	int nmax = -1;
	
	while (p != NULL) {
		if (p->naccess > nmax) {
			nmax = p->naccess;
			found = p;
		}

		p = p->next;
	}
	
	return (found);
}

void
sym_callchain_resort(sym_chainlist_t *list)
{
	sym_chainlist_t sortlist;
	sym_callchain_t *p;

	memset(&sortlist, 0, sizeof (sym_chainlist_t));

	while (list->num > 0) {
		if ((p = max_access_chain(list)) == NULL) {
			break;
		}

		/*
		 * Remove the found node from list
		 */
		chainlist_detach(list, p);
		chainlist_attach_tail(&sortlist, p);
	}

	sym_chainlist_free(list);
	memcpy(list, &sortlist, sizeof (sym_chainlist_t));
}

sym_callchain_t *
sym_callchain_detach(sym_chainlist_t *list)
{
	sym_callchain_t *chain;
	
	if ((chain = list->head) != NULL) {
		chainlist_detach(list, chain);
	}

	return (chain);
}

void
sym_callchain_free(sym_callchain_t *chain)
{
	if (chain->entry_arr != NULL) {
		free(chain->entry_arr);
	}
	
	free(chain);
}

void
sym_chainlist_free(sym_chainlist_t *list)
{
	sym_callchain_t *next, *p;
	
	p = list->head;
	while (p != NULL) {
		next = p->next;
		sym_callchain_free(p);
		p = next;
	}

	memset(list, 0, sizeof (sym_chainlist_t));
}

int
sym_chainlist_nentry(sym_chainlist_t *list, int *nchain)
{
	sym_callchain_t *next, *p;
	int nentry = 0;
	
	*nchain = 0;
	p = list->head;
	while (p != NULL) {
		next = p->next;
		*nchain += 1;
		nentry += p->nentry;
		p = next;
	}

	return (nentry);	
}
