/* Author: nibble 
 * --------------
 * Licensed under GPLv2
 * This file is part of radare
 */

#include "../main.h"
#include "dietelf_types.h"
#include "elf.h"

#undef ELF_
	
#ifdef DIETELF64
    #define ELF_(name) Elf64_##name 
#else       
    #define ELF_(name) Elf32_##name 
#endif      

#define ELF_ADDR_MASK   0xffffffffffff8000LL
#define ELF_GOTOFF_MASK 0xfffffffffffff000LL

#define ELF_SCN_IS_EXECUTABLE(x) x & SHF_EXECINSTR
#define ELF_SCN_IS_READABLE(x)   x & SHF_ALLOC
#define ELF_SCN_IS_WRITABLE(x)   x & SHF_WRITE

typedef struct {
    ELF_(Ehdr)  ehdr;
    ELF_(Phdr)* phdr;
    ELF_(Shdr)* shdr;
    int         plen;
    char**      section;
    char*       string;
    int         bss;
    u64         base_addr;
    const char* file;
} ELF_(dietelf_bin_t);


int   ELF_(dietelf_close)(int);
char* ELF_(dietelf_get_arch)(ELF_(dietelf_bin_t)*);
u64   ELF_(dietelf_get_base_addr)(ELF_(dietelf_bin_t)*);
char* ELF_(dietelf_get_data_encoding)(ELF_(dietelf_bin_t)*);
char* ELF_(dietelf_get_elf_class)(ELF_(dietelf_bin_t)*);
u64   ELF_(dietelf_get_entry_offset)(ELF_(dietelf_bin_t)*);
char* ELF_(dietelf_get_file_type)(ELF_(dietelf_bin_t)*);
int   ELF_(dietelf_get_imports)(ELF_(dietelf_bin_t)*, int, dietelf_import*);
int   ELF_(dietelf_get_imports_count)(ELF_(dietelf_bin_t)*, int);
int   ELF_(dietelf_get_libs)(ELF_(dietelf_bin_t)*, int, int, dietelf_string*);
char* ELF_(dietelf_get_machine_name)(ELF_(dietelf_bin_t)*);
char* ELF_(dietelf_get_osabi_name)(ELF_(dietelf_bin_t)*);
u64   ELF_(dietelf_get_section_index)(ELF_(dietelf_bin_t)*, int, const char*);
u64   ELF_(dietelf_get_section_offset)(ELF_(dietelf_bin_t)*, int, const char*);
int   ELF_(dietelf_get_section_size)(ELF_(dietelf_bin_t)*, int, const char*);
int   ELF_(dietelf_get_sections)(ELF_(dietelf_bin_t)*, int, dietelf_section*);
int   ELF_(dietelf_get_sections_count)(ELF_(dietelf_bin_t)*);
int   ELF_(dietelf_get_static)(ELF_(dietelf_bin_t)*);
int   ELF_(dietelf_get_strings)(ELF_(dietelf_bin_t)*, int, int, int, dietelf_string*);
int   ELF_(dietelf_get_stripped)(ELF_(dietelf_bin_t)*);
int   ELF_(dietelf_get_symbols)(ELF_(dietelf_bin_t)*, int, dietelf_symbol*);
int   ELF_(dietelf_get_symbols_count)(ELF_(dietelf_bin_t)*, int);
int   ELF_(dietelf_is_big_endian)(ELF_(dietelf_bin_t)*);
int   ELF_(dietelf_open)(ELF_(dietelf_bin_t)*, const char*);
