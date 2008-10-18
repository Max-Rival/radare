/* Author: nibble 
 * --------------
 * Licensed under GPLv2
 * This file is part of radare
 */

#ifndef _INCLUDE_DIETELF_TYPES_H_
#define _INCLUDE_DIETELF_TYPES_H_

#include "../main.h"
#include "elf.h"

typedef struct {
	u64 offset;
	u64 size;
	u64 align;
	u32 flags;
	char name[ELF_NAME_LENGTH];
} dietelf_section;

typedef struct {
	u64 offset;
	char bind[ELF_NAME_LENGTH];
	char type[ELF_NAME_LENGTH];
	char name[ELF_NAME_LENGTH];
} dietelf_import;

typedef struct {
	u64 offset;
	u32 size;
	char bind[ELF_NAME_LENGTH];
	char type[ELF_NAME_LENGTH];
	char name[ELF_NAME_LENGTH];
} dietelf_symbol;

/*
typedef struct {
} dietelf_export;
*/
#endif
