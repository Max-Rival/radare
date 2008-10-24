/*
 * Copyright (C) 2007, 2008
 *       pancake <youterm.com>
 *       nibble
 *
 * radare is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * radare is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with radare; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "../main.h"
#include "rabin.h"
#include <stdio.h>
#if __UNIX__
#include <fcntl.h>
#include <sys/mman.h>
#include <dlfcn.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "aux.h"

#include "dietelf.h"
#include "dietelf64.h"
#include "dietelf_types.h"
#include "dietpe.h"
#include "dietpe_types.h"
#include "dietmach0.h"

#define ELF_CALL(func, bin, args...) elf64?Elf64_##func(&bin.e64,##args):Elf32_##func(&bin.e32,##args)

typedef union {
    Elf32_dietelf_bin_t e32;
    Elf64_dietelf_bin_t e64;
} dietelf_bin_t;

// TODO : move into rabin_t
char *file = NULL;
int filetype = FILETYPE_UNK;
int action   = ACTION_UNK;
int verbose  = 0;
int xrefs    = 0;
int rad      = 0; //radare output format
int fd       = -1;
static int pebase = 0;
static int elf64 = 0;

int rabin_show_help()
{
	printf(
			"rabin [options] [bin-file]\n"
//			" -a        show arch\n"
			" -e        shows entrypoints one per line\n"
			" -i        imports (symbols imported from libraries)\n"
			" -s        symbols (exports)\n"
			" -c        header checksum\n"
			" -S        show sections\n"
			" -l        linked libraries\n"
			" -L [lib]  dlopen library and show address\n"
			" -z        search for strings in elf non-executable sections\n"
			" -x        show xrefs of symbols (-s/-i/-z required)\n"
			" -I        show binary info\n"
			" -r        output in radare commands\n"
			" -v        be verbose\n");
	return 1;
}

void rabin_show_info(const char *file)
{
	char *str, pe_os_str[PE_NAME_LENGTH], pe_arch_str[PE_NAME_LENGTH];
	char pe_class_str[PE_NAME_LENGTH], pe_subsystem_str[PE_NAME_LENGTH], pe_machine_str[PE_NAME_LENGTH];
	int pe_os, pe_arch, pe_class, pe_subsystem, pe_machine;
	u64 baddr;
	union {
		dietelf_bin_t elf;
		dietpe_bin pe;
	} bin;
	dietpe_entrypoint entrypoint;

	switch(filetype) {
	case FILETYPE_ELF:
		fd = ELF_CALL(dietelf_open,bin.elf,file);
		if (fd == -1) {
			fprintf(stderr, "cannot open file\n");
			return;
		}

		baddr = ELF_CALL(dietelf_get_base_addr, bin.elf);

		if (!rad)
			printf("[Information]\n");

		if (rad) {
			printf("e file.type = elf\n");
			str = getenv("DEBUG");
			if (str == NULL || (str && strncmp(str, "1", 1)))
				printf("e file.baddr = 0x%08llx\n", baddr);
			if (ELF_CALL(dietelf_is_big_endian,bin.elf))
				printf("e cfg.bigendian = true\n");
			else printf("e cfg.bigendian = false\n");
			if (ELF_CALL(dietelf_get_stripped,bin.elf))
			    printf("e dbg.dwarf = false\n");
			else printf("e dbg.dwarf = true\n");
			printf("e asm.os = %s\n", ELF_CALL(dietelf_get_osabi_name,bin.elf));
			printf("e asm.arch = %s\n", ELF_CALL(dietelf_get_arch,bin.elf));
		} else {
			switch (verbose) {
				case 0:
					printf("class=%s\nenconding=%s\nos=%s\nmachine=%s\narch=%s\ntype=%s\nstripped=%s\nstatic=%s\nbaddr=0x%08llx\n",
							ELF_CALL(dietelf_get_elf_class,bin.elf),
							ELF_CALL(dietelf_get_data_encoding,bin.elf),
							ELF_CALL(dietelf_get_osabi_name,bin.elf),
							ELF_CALL(dietelf_get_machine_name,bin.elf),
							ELF_CALL(dietelf_get_arch,bin.elf),
							ELF_CALL(dietelf_get_file_type,bin.elf),
							(ELF_CALL(dietelf_get_stripped,bin.elf))?"Yes":"No",
						  	(ELF_CALL(dietelf_get_static,bin.elf))?"Yes":"No",
							baddr);
					break;
				default :
					printf("ELF class:       %s\n"
							"Data enconding:  %s\n"
							"OS/ABI name:     %s\n"
							"Machine name:    %s\n"
							"Architecture:    %s\n"
							"File type:       %s\n"
							"Stripped:        %s\n"
							"Static:          %s\n"
							"Base address:    0x%08llx\n",
							ELF_CALL(dietelf_get_elf_class,bin.elf),
							ELF_CALL(dietelf_get_data_encoding,bin.elf),
							ELF_CALL(dietelf_get_osabi_name,bin.elf),
							ELF_CALL(dietelf_get_machine_name,bin.elf),
							ELF_CALL(dietelf_get_arch,bin.elf),
							ELF_CALL(dietelf_get_file_type,bin.elf),
							(ELF_CALL(dietelf_get_stripped,bin.elf))?"Yes":"No",
						  	(ELF_CALL(dietelf_get_static,bin.elf))?"Yes":"No",
							baddr);
			}
		}

		ELF_(dietelf_close)(fd);
		break;
	case FILETYPE_CLASS:
		if (rad) {
			printf("e asm.arch = java\n");
			printf("e cfg.bigendian = true\n");
		} else printf("File type: JAVA CLASS\n");
		break;
	case FILETYPE_PE:
		if ((fd = dietpe_open(&bin.pe, file)) == -1) {
			fprintf(stderr, "Cannot open file\n");
			return;
		}

		dietpe_get_entrypoint(&bin.pe, &entrypoint);
		pe_os = dietpe_get_os(&bin.pe, pe_os_str);
		pe_arch = dietpe_get_arch(&bin.pe, pe_arch_str);
		pe_class = dietpe_get_class(&bin.pe, pe_class_str);
		pe_machine = dietpe_get_machine(&bin.pe, pe_machine_str);
		pe_subsystem = dietpe_get_subsystem(&bin.pe, pe_subsystem_str);

		if (!rad)
			printf("[Information]\n");


		if (rad) {
			printf("e file.type = pe\n");
			str = getenv("DEBUG");
			if (str == NULL || (str && strncmp(str, "1", 1)))
				printf("e file.baddr = 0x%08llx\n", (u64) dietpe_get_image_base(&bin.pe));
				printf("e cfg.bigendian = %s\n",
						dietpe_is_big_endian(&bin.pe)?"true":"false");
				printf("e asm.os = %s\n", pe_os_str);
				printf("e asm.arch = %s\n", pe_arch_str);
		} else { 
			switch (verbose) {
				case 0:
					printf("class=%s\n"
							"dll=%s\n"
							"machine=%s\n"
							"big_endian=%s\n"
							"subsystem=%s\n"
							"relocs=%s\n"
							"line_nums=%s\n"
							"local_syms=%s\n"
							"debug=%s\n"
							"number_of_sections=%i\n"
							"baddr=0x%08llx\n"
							"section_alignment=%i\n"
							"file_alignment=%i\n"
							"image_size=%i\n",
							pe_class_str, dietpe_is_dll(&bin.pe)?"True":"False", pe_machine_str,
							dietpe_is_big_endian(&bin.pe)?"True":"False", pe_subsystem_str,
							dietpe_is_stripped_relocs(&bin.pe)?"True":"False", dietpe_is_stripped_line_nums(&bin.pe)?"True":"False",
							dietpe_is_stripped_local_syms(&bin.pe)?"True":"False", dietpe_is_stripped_debug(&bin.pe)?"True":"False",
							dietpe_get_sections_count(&bin.pe), (u64) dietpe_get_image_base(&bin.pe),
							dietpe_get_section_alignment(&bin.pe), dietpe_get_file_alignment(&bin.pe), dietpe_get_image_size(&bin.pe));
					break;
				default:
					printf("PE Class: %s (0x%x)\n"
							"DLL: %s\n"
							"Machine: %s (0x%x)\n"
							"Big endian: %s\n"
							"Subsystem: %s (0x%x)\n"
							"Stripped:\n"
							"  - Relocs: %s\n"
							"  - Line numbers: %s\n"
							"  - Local symbols: %s\n"
							"  - Debug: %s\n"
							"Number of sections: %i\n"
							"Image base: 0x%08x\n"
							"Section alignment: %i\n"
							"File alignment: %i\n"
							"Image size: %i\n",
							pe_class_str, pe_class, dietpe_is_dll(&bin.pe)?"True":"False", pe_machine_str, pe_machine,
							dietpe_is_big_endian(&bin.pe)?"True":"False", pe_subsystem_str, pe_subsystem,
							dietpe_is_stripped_relocs(&bin.pe)?"True":"False", dietpe_is_stripped_line_nums(&bin.pe)?"True":"False",
							dietpe_is_stripped_local_syms(&bin.pe)?"True":"False", dietpe_is_stripped_debug(&bin.pe)?"True":"False",
							dietpe_get_sections_count(&bin.pe), dietpe_get_image_base(&bin.pe),
							dietpe_get_section_alignment(&bin.pe), dietpe_get_file_alignment(&bin.pe), dietpe_get_image_size(&bin.pe));
			}
		}

		dietpe_close(fd);
		break;
	case FILETYPE_MZ:
		if (rad) printf("e file.type = mz\n");
		else printf("File type: DOS COM\n");
		break;
	case FILETYPE_DEX:
		if (!rad)
			printf("File type: DEX (google android)\n");
		break;
	case FILETYPE_BF:
		if (rad)
			printf("e asm.arch = bf\n");
		else printf("File type: Brainfuck\n");
		break;
	case FILETYPE_MACHO:
		if (rad) {
			printf("e file.type = macho\n");
			printf("e asm.arch = ppc\n");
			printf("e cfg.bigendian = false\n");
		} else printf("File type: MACH-O\n");
		break;
	case FILETYPE_CSRFW:
		if (rad)
			printf("e asm.arch = csr\n");
		else printf("File type: CSR firmware\n");
		break;
	case FILETYPE_ARMFW:
		if (rad)
			printf("e asm.arch = arm\n");
		else printf("File type: ARM firmwarre\n");
		break;
	case FILETYPE_UNK:
		if (rad) printf("e file.type = unk\n");
		else printf("File type: UNKNOWN\n");
		break;
	}
}

void rabin_show_strings(const char *file)
{
	union {
		dietelf_bin_t elf;
		dietpe_bin pe;
	} bin;
	union {
		dietelf_string* elf;
		dietpe_string* pe;
	} strings, stringsp;
	u64 baddr = 0;
	int strings_count, i;
	char buf[1024];

	if (xrefs) {
		snprintf(buf,1023, "printf \"pC @@ str_\\nq\\ny\\n\" | radare -n -e file.id=1 -e file.flag=1 -e file.analyze=1 -vd %s", file);
		system(buf);
		return;
	}
	switch(filetype) {
	case FILETYPE_ELF:
		fd = ELF_CALL(dietelf_open,bin.elf,file);
		if (fd == -1) {
			fprintf(stderr, "cannot open file\n");
			return;
		}

		baddr = ELF_CALL(dietelf_get_base_addr,bin.elf);
		strings.elf = malloc(4096 * sizeof(dietelf_string));
		strings_count = ELF_CALL(dietelf_get_strings,bin.elf,fd,verbose,4096,strings.elf);

		if (rad)
			printf("fs strings\n");
		else printf("[Strings]\n");

		stringsp = strings;
		for (i = 0; i < strings_count; i++, stringsp.elf++) {
			if (rad) {
				printf("b %lli && f str_%s @ 0x%08llx\n",
						stringsp.elf->size, aux_filter_rad_output(stringsp.elf->string), baddr + stringsp.elf->offset);
				printf("Cs %lli @ 0x%08llx\n", stringsp.elf->size, baddr + stringsp.elf->offset);
			} else {
				switch (verbose) {
					case 0:
						printf("address=0x%08llx offset=0x%08llx size=%08lli type=%c name=%s\n",
								baddr + stringsp.elf->offset, stringsp.elf->offset, stringsp.elf->size, stringsp.elf->type, stringsp.elf->string);
						break;
					case 1:
						if (i == 0) printf("Memory address\tFile offset\tName\n");
						printf("0x%08llx\t0x%08llx\t%s\n",
								baddr + stringsp.elf->offset, stringsp.elf->offset, stringsp.elf->string);
						break;
					default:
						if (i == 0) printf("Memory address\tFile offset\tSize\t\tType\tName\n");
						printf("0x%08llx\t0x%08llx\t%08lli\t%c\t%s\n",
								baddr + stringsp.elf->offset, stringsp.elf->offset, stringsp.elf->size, stringsp.elf->type, stringsp.elf->string);
				}
			}
		}

		if (rad) {
			printf("b 512\n");
			fprintf(stderr, "%i strings added\n", strings_count);
		} else if (verbose != 0) 
			printf("\n%i strings\n", strings_count);

		ELF_(dietelf_close)(fd);
		break;
	case FILETYPE_PE:
#if 1
		// TODO: native version and support for non -r
		snprintf(buf, 1022, "rsc strings-pe-flag %s",file);
		system(buf);
#else
		if ((fd = dietpe_open(&bin.pe, file)) == -1) {
			fprintf(stderr, "Cannot open file\n");
			return;
		}

		baddr = dietpe_get_image_base(&bin.pe);

		strings.pe = malloc(4096 * sizeof(dietpe_string));
		strings_count = dietpe_get_strings(&bin.pe, fd, verbose, 4096, strings.pe);

		if (rad)
			printf("fs strings\n");
		else printf("[Strings]\n");

		stringsp.pe = strings.pe;
		for (i = 0; i < strings_count; i++, stringsp.pe++) {
			if (rad) {
				printf("b %lli && f str_%s @ 0x%08llx\n",
						(u64) stringsp.pe->size, aux_filter_rad_output(stringsp.pe->string), (u64) (baddr + stringsp.pe->rva));
				printf("Cs %lli @ 0x%08llx\n", (u64) stringsp.pe->size, (u64) (baddr + stringsp.pe->rva));
			} else {
				switch (verbose) {
					case 0:
						printf("address=0x%08llx offset=0x%08llx size=%08lli type=%c name=%s\n",
								(u64) (baddr + stringsp.pe->rva), (u64) stringsp.pe->offset, (u64) stringsp.pe->size, stringsp.pe->type, stringsp.pe->string);
						break;
					case 1:
						if (i == 0) printf("Memory address\tFile offset\tName\n");
						printf("0x%08llx\t0x%08llx\t%s\n",
								(u64) (baddr + stringsp.pe->rva), (u64) stringsp.pe->offset, stringsp.pe->string);
						break;
					default:
						if (i == 0) printf("Memory address\tFile offset\tSize\t\tType\tName\n");
						printf("0x%08llx\t0x%08llx\t%08lli\t%c\t%s\n",
								(u64) (baddr + stringsp.pe->rva), (u64) stringsp.pe->offset, (u64) stringsp.pe->size, stringsp.pe->type, stringsp.pe->string);
				}
			}
		}


		if (rad) {
			printf("b 512\n");
			fprintf(stderr, "%i strings added\n", strings_count);
		} else if (verbose != 0) 
			printf("\n%i strings\n", strings_count);

		dietpe_close(fd);
#endif
		break;
	case FILETYPE_BF:
		/* do nothing */
		break;
	default:
		snprintf(buf, 1022, "echo /s | radare -nv %s",file);
		system(buf);
		break;
	}
	
	//sprintf(buf, "echo /s | radare -e file.id=true -nv %s", file);
	//system(buf);
}

void rabin_show_checksum(const char *file)
{
	unsigned char buf[32];
	unsigned long addr = 0;
	int i;

	switch(filetype) {
	case FILETYPE_DEX:
		lseek(fd, 8, SEEK_SET);
		read(fd, &addr, 4);
		printf("Checksum: 0x%08lx\n", addr);
		read(fd, &buf, 20);
		printf("SHA-1 Signature: ");
		for(i=0;i<20;i++)
			printf("%08x ", buf[i]);
		break;
	case FILETYPE_ELF:
		break;
	case FILETYPE_MZ:
		break;
	case FILETYPE_PE:
		lseek(fd, pebase+0x18, SEEK_SET);
		read(fd, &addr, 4);
		printf("0x%x checksum file offset\n", pebase+0x18);
		printf("0x%04x checksum\n", (unsigned int) (unsigned short)addr);
		break;
	}
}

void rabin_show_entrypoint()
{
	u64 offset = 0;
	u64 baddr = 0;
	dietpe_entrypoint entrypoint;
	union {
		dietelf_bin_t elf;
		dietpe_bin    pe;
	} bin;


	switch(filetype) {
	case FILETYPE_ELF:
		/* pW 4 @ 0x18 */
		fd = ELF_CALL(dietelf_open,bin.elf, file);
		if (fd == -1) {
			fprintf(stderr, "cannot open file\n");
			return;
		}

		offset = ELF_CALL(dietelf_get_entry_offset, bin.elf);
		baddr = ELF_CALL(dietelf_get_base_addr, bin.elf);

		if (rad) {
			printf("fs symbols\n");
		} else if (verbose != 0)
			printf("[Entrypoint]\n");

		if (rad) {
			printf("f entrypoint @ 0x%08llx\n", baddr + offset);
			printf("s entrypoint\n");
		} else {
			switch (verbose) {
				case 0:
					printf("0x%08llx\n", baddr + offset);
					break;
				case 1:
					printf("Memory address:\t0x%08llx\n", baddr + offset);
					break;
				default:
					printf("Memory address:\t0x%08llx\n", baddr + offset);
					printf("File offset:\t0x%08llx\n", offset);
			}
		}

		ELF_(dietelf_close)(fd);
		break;
	case FILETYPE_PE:
#if 0
		lseek(fd, pebase+0x28, SEEK_SET);
		read(fd, &addr, 4);
	//	printf("0x%08x disk offset for ep\n", pebase+0x28);
		lseek(fd, pebase+0x45, SEEK_SET);
		read(fd, &base, 4);
		if (rad) {
			printf("f entrypoint @ 0x%08llx\n", addr);
		} else {
			if (verbose) {
				printf("0x%08llx memory\n", base+addr);
				printf("0x%08llx disk\n", addr-0xc00);
			} else	printf("0x%08llx\n", base+addr);
		}
#endif
		if ((fd = dietpe_open(&bin.pe, file)) == -1) {
			fprintf(stderr, "cannot open file\n");
			return;
		}

		baddr = dietpe_get_image_base(&bin.pe);
		dietpe_get_entrypoint(&bin.pe, &entrypoint);

		if (rad) {
			printf("fs symbols\n");
		} else if (verbose != 0)
			printf("[Entrypoint]\n");

		if (rad) {
			printf("f entrypoint @ 0x%08llx\n", (u64) (baddr + entrypoint.rva));
			printf("s entrypoint\n");
		} else {
			switch (verbose) {
				case 0:
					printf("0x%08llx\n", (u64) (baddr + entrypoint.rva));
					break;
				case 1:
					printf("Memory address:\t0x%08llx\n", (u64) (baddr + entrypoint.rva));
					break;
				default:
					printf("Memory address:\t0x%08llx\n", (u64) (baddr + entrypoint.rva));
					printf("File offset:\t0x%08llx\n", (u64) (entrypoint.offset));
			}
		}
		

		dietpe_close(fd);
		break;
	case FILETYPE_MACHO:
		printf("fs symbols\n");
#if 0
		/* TODO: Walk until LOAD COMMAND 9 */
Load command 9
        cmd LC_UNIXTHREAD
    cmdsize 80
     flavor i386_THREAD_STATE
      count i386_THREAD_STATE_COUNT
            eax 0x00000000 ebx    0x00000000 ecx 0x00000000 edx 0x00000000
            edi 0x00000000 esi    0x00000000 ebp 0x00000000 esp 0x00000000
            ss  0x00000000 eflags 0x00000000 eip 0x000023d0 cs  0x00000000
            ds  0x00000000 es     0x00000000 fs  0x00000000 gs  0x00000000
#endif
		{
		char buf[256];
		sprintf(buf, "otool -l %s | grep eip | awk '{print $6}'", file);
		system(buf);
		}
		break;
	case FILETYPE_BF:
		/* skip invalid chars */
		if (rad)
			printf("f entrypoint @ 0\n");
		else	printf("Entrypoint: 0\n");
		break;
	case FILETYPE_MZ:
		break;
	}
}

u64 addr_for_lib(char *name)
{
#if __UNIX__
	u32 *addr = dlopen(name, RTLD_LAZY);
	if (addr) {
		dlclose(addr);
		return (u64)((addr!=NULL)?(addr):0LL);
	} else {
		printf("cannot open '%s' library\n", name);
		return 0LL;
	}
#endif
	return 0LL;
}

#if 0
void rabin_show_arch(char *file)
{
	union {
		dietelf_bin_t elf;
		dietpe_bin    pe;
	} bin;
	u32 dw;
	u16 w;

	switch(filetype) {
	case FILETYPE_MACHO:
		dm_read_header(1);
		break;
	case FILETYPE_ELF:
		fd = ELF_CALL(dietelf_open,bin.elf,file);
		if (fd == -1) {
			fprintf(stderr, "cannot open file\n");
			return;
		}

		if (!rad) printf("arch: %s\n", ELF_CALL(dietelf_get_arch,bin.elf));

		ELF_(dietelf_close)(fd);
		break;
	case FILETYPE_PE:
		// [[0x3c]+4]
		lseek(fd, 0x3c, SEEK_SET);
		read(fd, &dw, 4);
		lseek(fd, dw+4, SEEK_SET);
		read(fd, &w, 2);
		switch(w) {
		case 0x1c0:
			printf("arch: ARM\n");
			break;
		case 0x14c:
			printf("arch: x86-32\n");
			break;
		default:
			printf("arch: 0x%x (unknown)\n", w);
		}
		break;
	}
}
#endif

void rabin_show_imports(const char *file)
{
	char buf[1024];
	int i, imports_count;
	u64 baddr = 0;
	union {
		dietelf_bin_t elf;
		dietpe_bin    pe;
	} bin;
	union {
		dietelf_import* elf;
		dietpe_import*  pe;
	} import, importp;

	if (xrefs) {
		snprintf(buf,1023, "printf \"pC @@ imp_\\nq\\ny\\n\" | radare -n -e file.id=1 -e file.flag=1 -e file.analyze=1 -vd %s", file);
		system(buf);
		return;
	}

	switch(filetype) {
	case FILETYPE_ELF:
#if 0
		{ char buf[1024];
		//sprintf(buf, "readelf -sA '%s'|grep GLOBAL | awk ' {print $8}'", file);
//		sprintf(buf, "readelf -s '%s' | grep FUNC | grep GLOBAL | grep DEFAULT  | grep ' UND ' | awk '{ print \"0x\"$2\" \"$8 }' | sort | uniq" , file);
		sprintf(buf, "objdump -d '%s' | grep 'plt>:' | sed -e 's,@plt>:,,g' -e 's,[<],,g' | awk '{print \"f imp_\"$2\" @ 0x\"$1 }'", file);
		system(buf);
		}
#else
		fd = ELF_CALL(dietelf_open,bin.elf,file);
		if (fd == -1) {
			fprintf(stderr, "cannot open file\n");
			return;
		}
		
		baddr = ELF_CALL(dietelf_get_base_addr,bin.elf);
		imports_count = ELF_CALL(dietelf_get_imports_count,bin.elf,fd);

		import.elf = malloc(imports_count * sizeof(dietelf_import));
		ELF_CALL(dietelf_get_imports,bin.elf,fd,import.elf);

		if (rad)
			printf("fs imports\n");
		else printf("[Imports]\n");

		importp.elf = import.elf;
		for (i = 0; i < imports_count; i++, importp.elf++) {
			if (rad) {
				printf("f imp_%s @ 0x%08llx\n", aux_filter_rad_output(importp.elf->name), baddr + importp.elf->offset);
			} else {
				switch (verbose) {
					case 0:
						printf("address=0x%08llx offset=0x%08llx bind=%s type=%s name=%s\n",
								baddr + importp.elf->offset, importp.elf->offset,
								importp.elf->bind, importp.elf->type, importp.elf->name);
						break;
					case 1:
						if (i == 0) printf("Memory address\tFile offset\tName\n");
						printf("0x%08llx\t0x%08llx\t%s\n", baddr + importp.elf->offset, importp.elf->offset, importp.elf->name);
						break;
					default:
						if (i == 0) printf("Memory address\tFile offset\tBind\tType\tName\n");
						printf("0x%08llx\t0x%08llx\t%-7s\t%-7s\t%s\n",
								baddr + importp.elf->offset, importp.elf->offset,
								importp.elf->bind, importp.elf->type, importp.elf->name);
				}
			}
		}

		if (rad) {
			printf("b 512\n");
			fprintf(stderr, "%i imports added\n", imports_count);
		} else if (verbose != 0) 
			printf("\n%i imports\n", imports_count);

		ELF_(dietelf_close)(fd);
#endif
		break;
	case FILETYPE_MACHO:
		setenv("target", file, 1);
		if (rad) {
			printf("fs imports\n");
			fflush(stdout);
			system("otool -vI $target | grep 0x | awk '{ print \"f imp_\"$3\" @ \"$1 }'");
		} else {
			system("otool -vI $target | grep 0x");
#if 0
		   #if __DARWIN_BYTE_ORDER
			sprintf(buf, "nm '%s' | grep ' T ' | sed 's/ T / /' | awk '{print \"0x\"$1\" \"$2}'", file);
			system(buf);
		   #else
			sprintf(buf, "arm-apple-darwin-nm '%s' | grep ' T ' | sed 's/ T / /' | awk '{print \"0x\"$1\" \"$2}'", file);
			system(buf);
		   #endif
#endif
		}
		break;
	case FILETYPE_PE:
		if ((fd = dietpe_open(&bin.pe, file)) == -1) {
			fprintf(stderr, "Cannot open file\n");
			return;
		}

		baddr = dietpe_get_image_base(&bin.pe);
		imports_count = dietpe_get_imports_count(&bin.pe, fd);

		import.pe = malloc(imports_count * sizeof(dietpe_import));
		dietpe_get_imports(&bin.pe, fd, import.pe);

		if (rad)
			printf("fs imports\n");
		else printf("[Imports]\n");
		
		importp.pe = import.pe;
		for (i = 0; i < imports_count; i++, importp.pe++) {
			if (rad) {
				printf("f imp_%s @ 0x%08llx\n", aux_filter_rad_output(importp.pe->name), (u64) (baddr + importp.pe->rva));
			} else {
				switch (verbose) {
					case 0:
						printf("address=0x%08llx offset=0x%08llx hint=%04i ordinal=%04i %s\n",
								(u64) (baddr + importp.pe->rva), (u64) importp.pe->offset, importp.pe->hint, importp.pe->ordinal, importp.pe->name);
						break;
					case 1:
						if (i == 0) printf("Memory address\tFile offset\tName\n");
						printf("0x%08llx\t0x%08llx\t%s\n",
								(u64) (baddr + importp.pe->rva), (u64) importp.pe->offset, importp.pe->name);
						break;
					default:
						if (i == 0) printf("Memory address\tFile offset\tHint\tOrdinal\tName\n");
						printf("0x%08llx\t0x%08llx\t%04i\t%04i\t%s\n",
								(u64) (baddr + importp.pe->rva), (u64) importp.pe->offset, importp.pe->hint, importp.pe->ordinal, importp.pe->name);
				}
			}
		}

		if (rad) {
			printf("b 512\n");
			fprintf(stderr, "%i imports added\n", imports_count);
		} else if (verbose != 0) 
			printf("\n%i imports\n", imports_count);

		dietpe_close(fd);
		break;
	}
}

void rabin_show_symbols(char *file)
{
	char buf[1024];
	u64 baddr = 0;
	union {
		dietelf_bin_t elf;
		dietpe_bin    pe;
	} bin;
	union {
		dietelf_symbol* elf;
		dietpe_export*  pe;
	} symbol, symbolp;
	int symbols_count, i;

	switch(filetype) {
	case FILETYPE_ELF:
#if 0		
		sprintf(buf, "readelf -s '%s' | grep FUNC | grep GLOBAL | grep DEFAULT  | grep ' 12 ' | awk '{ print \"0x\"$2\" \"$8 }' | sort | uniq" , file);
		system(buf);
#endif		
		fd = ELF_CALL(dietelf_open,bin.elf,file);
		if (fd == -1) {
			fprintf(stderr, "cannot open file\n");
			return;
		}

		baddr = ELF_CALL(dietelf_get_base_addr,bin.elf);
		symbols_count = ELF_CALL(dietelf_get_symbols_count,bin.elf,fd);

		symbol.elf = malloc(symbols_count * sizeof(dietelf_symbol));
		ELF_CALL(dietelf_get_symbols,bin.elf,fd,symbol.elf);

		if (rad)
			printf("fs symbols\n");
		else printf("[Symbols]\n");

		symbolp.elf = symbol.elf;
		for (i = 0; i < symbols_count; i++, symbolp.elf++) {
			if (rad) {
				printf("b %lli && f sym_%s @ 0x%08llx\n", symbolp.elf->size, aux_filter_rad_output(symbolp.elf->name), baddr + symbolp.elf->offset);
				if (!strncmp(symbolp.elf->type,"FUNC", 4)) 
					printf("CF %lli @ 0x%08llx\n", symbolp.elf->size, baddr + symbolp.elf->offset);
			} else {
				switch (verbose) {
					case 0:
						printf("address=0x%08llx offset=0x%08llx size=%08lli bind=%s type=%s name=%s\n",
								baddr + symbolp.elf->offset, symbolp.elf->offset, symbolp.elf->size,
								symbolp.elf->bind, symbolp.elf->type, symbolp.elf->name);
						break;
					case 1:
						if (i == 0) printf("Memory address\tFile offset\tName\n");
						printf("0x%08llx\t0x%08llx\t%s\n",
								baddr + symbolp.elf->offset, symbolp.elf->offset, symbolp.elf->name);
						break;
					default:
						if (i == 0) printf("Memory address\tFile offset\tSize\t\tBind\tType\tName\n");
						printf("0x%08llx\t0x%08llx\t%08lli\t%-7s\t%-7s\t%s\n",
								baddr + symbolp.elf->offset, symbolp.elf->offset, symbolp.elf->size,
								symbolp.elf->bind, symbolp.elf->type, symbolp.elf->name);
				}
			}
		}

		if (rad) {
			printf("b 512\n");
			fprintf(stderr, "%i symbols added\n", symbols_count);
		} else if (verbose != 0)
			printf("\n%i symbols\n", symbols_count);

		ELF_(dietelf_close)(fd);
		break;
	case FILETYPE_MACHO:
		setenv("target", file, 1);
		if (rad) {
			printf("fs symbols\n");
			fflush(stdout);
			system("otool -tv $target | grep -C 1 -e : | grep -v / | awk '{if (/:/){label=$1;gsub(\":\",\"\",label);next}if (label!=\"\"){print \"f sym\"label\" @ 0x\"$1;label=\"\"}}'");
		} else {
		   #if __DARWIN_BYTE_ORDER
			sprintf(buf, "nm '%s' | grep ' T ' | sed 's/ T / /' | awk '{print \"0x\"$1\" \"$2}'", file);
			system(buf);
		   #else
			sprintf(buf, "arm-apple-darwin-nm '%s' | grep ' T ' | sed 's/ T / /' | awk '{print \"0x\"$1\" \"$2}'", file);
			system(buf);
		   #endif
		}
		break;
	case FILETYPE_CLASS:
		// TODO: native version and support for non -r
		if (rad)
			snprintf(buf, 1022, "javasm -rc %s",file);
		else
			snprintf(buf, 1022, "javasm -c '%s'", file);
		system(buf);
		break;
	case FILETYPE_PE:
		if ((fd = dietpe_open(&bin.pe, file)) == -1) {
			fprintf(stderr, "Cannot open file\n");
			return;
		}

		symbols_count = dietpe_get_exports_count(&bin.pe, fd);
		baddr = dietpe_get_image_base(&bin.pe);

		symbol.pe = malloc(symbols_count * sizeof(dietpe_export));
		dietpe_get_exports(&bin.pe, fd, symbol.pe);

		if (rad)
			printf("fs symbols\n");
		else printf("[Symbols]\n");

		symbolp.pe = symbol.pe;
		for (i = 0; i < symbols_count; i++, symbolp.pe++) {
			if (rad) {
				printf("f sym_%s @ 0x%08llx\n", aux_filter_rad_output(symbolp.pe->name), (u64) (baddr + symbolp.pe->rva));
			} else {
				switch (verbose) {
					case 0:
						printf("address=0x%08llx offset=0x%08llx ordinal=%03i forwarder=%s %s\n", (u64) (baddr + symbolp.pe->rva), (u64) symbolp.pe->offset, symbolp.pe->ordinal, symbolp.pe->forwarder, symbolp.pe->name);
						break;
					case 1:
						if (i == 0) printf("Memory address\tFile offset\tName\n");
						printf("0x%08llx\t%08llx\t%s\n", (u64) (baddr + symbolp.pe->rva), (u64) symbolp.pe->offset, symbolp.pe->name);
						break;
					default:
						if (i == 0) printf("Memory address\tFile offset\tOrdinal\tForwarder\t\tName\n");
						printf("0x%08llx\t0x%08llx\t%03i\t%-16s\t%s\n", (u64) (baddr + symbolp.pe->rva), (u64) symbolp.pe->offset, symbolp.pe->ordinal, symbolp.pe->forwarder, symbolp.pe->name);
				}
			}
		}

		if (rad) {
			printf("b 512\n");
			fprintf(stderr, "%i symbols added\n", symbols_count);
		} else if (verbose != 0)
			printf("\n%i symbols\n", symbols_count);

		dietpe_close(fd);
		break;
	}
}

void rabin_show_sections(const char *file)
{
	int fd, i, sections_count;
	u64 baddr = 0;
	union {
		dietelf_bin_t elf;
		dietpe_bin    pe;
	} bin;
	union {
		dietelf_section* elf;
		dietpe_section*  pe;
	} section, sectionp;

	switch(filetype) {
	case FILETYPE_MACHO:
		dm_read_command(0);
		break;
	case FILETYPE_ELF:
		fd = ELF_CALL(dietelf_open,bin.elf,file);
		if (fd == -1) {
			fprintf(stderr, "cannot open file\n");
			return;
		}

		baddr = ELF_CALL(dietelf_get_base_addr,bin.elf);
		sections_count = ELF_CALL(dietelf_get_sections_count,bin.elf);

		section.elf = malloc(sections_count * sizeof(dietelf_section));
		ELF_CALL(dietelf_get_sections,bin.elf,fd,section.elf);

		if (rad)
			printf("fs sections\n");
		else printf("[Sections]\n");

		sectionp.elf = section.elf;
		for (i = 0; i < sections_count; i++, sectionp.elf++) {
			if (rad) {
				printf("f section_%s @ 0x%08llx\n", aux_filter_rad_output(sectionp.elf->name), (u64)(baddr + sectionp.elf->offset));
				printf("f section_%s_end @ 0x%08llx\n", aux_filter_rad_output(sectionp.elf->name), (u64)(baddr + sectionp.elf->offset + sectionp.elf->size));

				printf("CC [%02i] 0x%08llx size=%08lli align=0x%08llx %c%c%c %s @ 0x%08llx\n",
						i, baddr + sectionp.elf->offset, sectionp.elf->size,
						sectionp.elf->align,
						ELF_SCN_IS_READABLE(sectionp.elf->flags)?'r':'-',
						ELF_SCN_IS_WRITABLE(sectionp.elf->flags)?'w':'-',
						ELF_SCN_IS_EXECUTABLE(sectionp.elf->flags)?'x':'-',
						sectionp.elf->name, (u64)(baddr + sectionp.elf->offset));
			} else {
				switch (verbose) {
					case 0:
						printf("idx=%02i address=0x%08llx offset=0x%08llx size=%08lli align=0x%08llx privileges=%c%c%c name=%s\n",
								i, sectionp.elf->offset, baddr + sectionp.elf->offset,
								sectionp.elf->size,	sectionp.elf->align,
								ELF_SCN_IS_READABLE(sectionp.elf->flags)?'r':'-',
								ELF_SCN_IS_WRITABLE(sectionp.elf->flags)?'w':'-',
								ELF_SCN_IS_EXECUTABLE(sectionp.elf->flags)?'x':'-',
								sectionp.elf->name);
						break;
					case 1:
						if (i == 0) printf("Memory address\tFile offset\tName\n");
						printf("0x%08llx\t0x%08llx\t%s\n",
								sectionp.elf->offset, baddr + sectionp.elf->offset,
								sectionp.elf->name);
						break;
					default:
						if (i == 0) printf("Section index\tMemory address\tFile offset\tSize\t\tAlign\t\tPrivileges\tName\n");
						printf("%02i\t\t0x%08llx\t0x%08llx\t%08lli\t0x%08llx\t%c%c%c\t\t%s\n",
								i, sectionp.elf->offset, baddr + sectionp.elf->offset,
								sectionp.elf->size,	sectionp.elf->align,
								ELF_SCN_IS_READABLE(sectionp.elf->flags)?'r':'-',
								ELF_SCN_IS_WRITABLE(sectionp.elf->flags)?'w':'-',
								ELF_SCN_IS_EXECUTABLE(sectionp.elf->flags)?'x':'-',
								sectionp.elf->name);
				}
			}
		}

		if (rad) {
			printf("b 512\n");
			fprintf(stderr, "%i sections added\n", sections_count);
		} else if (verbose != 0){
			printf("\n%i sections\n", sections_count);
		}

		ELF_(dietelf_close)(fd);
		break;
	case FILETYPE_PE:
		if ((fd = dietpe_open(&bin.pe, file)) == -1) {
			fprintf(stderr, "Cannot open file\n");
			return;
		}
		
		sections_count = dietpe_get_sections_count(&bin.pe);
		baddr = dietpe_get_image_base(&bin.pe);

		section.pe = malloc(sections_count * sizeof(dietpe_section));
		dietpe_get_sections(&bin.pe, section.pe);

		if (rad)
			printf("fs sections\n");
		else printf("[Sections]\n");

		sectionp.pe = section.pe;
		for (i = 0; i < sections_count; i++, sectionp.pe++) {
			if (rad) {
				printf("f section_%s @ 0x%08llx\n", aux_filter_rad_output(sectionp.pe->name), (u64) (baddr + sectionp.pe->rva));
				printf("f section_%s_end @ 0x%08llx\n", aux_filter_rad_output(sectionp.pe->name), (u64)(baddr + sectionp.pe->rva + sectionp.pe->vsize));

				printf("CC [%02i] 0x%08llx size=%08lli %c%c%c%c %s @ 0x%08llx\n",
						i, (u64) (baddr + sectionp.pe->rva), (u64) (sectionp.pe->size),
						PE_SCN_IS_SHAREABLE(sectionp.pe->characteristics)?'s':'-',
						PE_SCN_IS_READABLE(sectionp.pe->characteristics)?'r':'-',
						PE_SCN_IS_WRITABLE(sectionp.pe->characteristics)?'w':'-',
						PE_SCN_IS_EXECUTABLE(sectionp.pe->characteristics)?'x':'-',
						sectionp.pe->name, (u64) (baddr + sectionp.pe->rva));
			} else {
				switch (verbose) {
					case 0:
						printf("idx=%02i address=0x%08llx offset=0x%08llx size=%08lli privileges=%c%c%c%c name=%s\n",
								i, (u64) (baddr + sectionp.pe->rva),
								(u64) (sectionp.pe->offset), (u64) (sectionp.pe->size),
								PE_SCN_IS_SHAREABLE(sectionp.pe->characteristics)?'s':'-',
								PE_SCN_IS_READABLE(sectionp.pe->characteristics)?'r':'-',
								PE_SCN_IS_WRITABLE(sectionp.pe->characteristics)?'w':'-',
								PE_SCN_IS_EXECUTABLE(sectionp.pe->characteristics)?'x':'-',
								sectionp.pe->name);
						break;
					case 1:
						if (i == 0) printf("Memory address\tFile offset\tName\n");
						printf("0x%08llx\t0x%08llx\t%s\n",
								(u64) (baddr + sectionp.pe->rva), (u64) (sectionp.pe->offset),
								sectionp.pe->name);
						break;
					default:
						if (i == 0) printf("Section index\tMemory address\tFile offset\tSize\t\tPrivileges\tName\n");
						printf("%02i\t\t0x%08llx\t0x%08llx\t%08lli\t%c%c%c%c\t\t%s\n",
								i, (u64) (baddr + sectionp.pe->rva),
								(u64) (sectionp.pe->offset), (u64) (sectionp.pe->size),
								PE_SCN_IS_SHAREABLE(sectionp.pe->characteristics)?'s':'-',
								PE_SCN_IS_READABLE(sectionp.pe->characteristics)?'r':'-',
								PE_SCN_IS_WRITABLE(sectionp.pe->characteristics)?'w':'-',
								PE_SCN_IS_EXECUTABLE(sectionp.pe->characteristics)?'x':'-',
								sectionp.pe->name);
				}
			}
		}

		if (rad) {
			printf("b 512\n");
			fprintf(stderr, "%i sections added\n", sections_count);
		} else if (verbose != 0){
			printf("\n%i sections\n", sections_count);
		}

		dietpe_close(fd);
		break;
#if 0
	default:
		// TODO: use the way that rsc flag-sections does
		char buf[1024];
		sprintf(buf, "readelf -S '%s'|grep '\\[' | grep -v '\\[Nr\\]' | cut -c 4- | awk '{ print \"0x\"$4\" \"$2 }'", file);
		system(buf);
#endif
	}
}

void rabin_show_libs(const char *file)
{
	char buf[1024];
	int fd;
	union {
		dietelf_bin_t elf;
		dietpe_bin pe;
	} bin;
	union {
		dietelf_string* elf;
		dietpe_string* pe;
	} libs, libsp;
	u64 baddr = 0;
	int i, libs_count;


	switch(filetype) {
	case FILETYPE_ELF:
		fd = ELF_CALL(dietelf_open,bin.elf,file);
		if (fd == -1) {
			fprintf(stderr, "cannot open file\n");
			return;
		}

		baddr = ELF_CALL(dietelf_get_base_addr,bin.elf);

		libs.elf = malloc(128*sizeof(dietelf_string));
		libs_count = ELF_CALL(dietelf_get_libs,bin.elf,fd,128,libs.elf);

		if (!rad)
			printf("[Libraries]\n");

		libsp.elf = libs.elf;
		for (i = 0; i < libs_count; i++, libsp.elf++) {
			if (!rad) {
				printf("%s\n", libsp.elf->string);
			}
		}

		if (!rad && verbose != 0) 
			printf("\n%i libraries\n", libs_count);

		ELF_(dietelf_close)(fd);
		break;
	case FILETYPE_PE:
		if ((fd = dietpe_open(&bin.pe, file)) == -1) {
			fprintf(stderr, "Cannot open file\n");
			return;
		}

		libs.pe = malloc(128*sizeof(dietpe_string));
		libs_count = dietpe_get_libs(&bin.pe, fd, 128, libs.pe);

		if (!rad)
			printf("[Libraries]\n");

		libsp.pe = libs.pe;
		for (i = 0; i < libs_count; i++, libs.pe++) {
			if (!rad) {
				printf("%s\n", libs.pe->string);
			}
		}

		if (!rad && verbose != 0) 
			printf("\n%i libraries\n", libs_count);

		ELF_(dietelf_close)(fd);

		break;
	case FILETYPE_MACHO:
		sprintf(buf, "otool -L '%s'", file);
		system(buf);
		break;
	default:
		sprintf(buf, "strings '%s' | grep -e '^lib'", file);
		system(buf);
		break;
	}
}

/* brainfuck header check */
int buf_is_bf(const u8 * buf, int len)
{
	int i;
	for(i=0;i<len;i++) {
		switch(buf[i]) {
		case '<':
		case '>':
		case '+':
		case '-':
		case '.':
		case ',':
		case '[':
		case ']':
			break;
		default:
			return 0;
		}
	}
	return 1;
}

int rabin_identify_header()
{
	unsigned char buf[1024];

	lseek(fd, 0, SEEK_SET);
	read(fd, buf, 1024);

	if ( !memcmp(buf, "\xCA\xFE\xBA\xBE", 4) ) {
		if (buf[9])
			filetype = FILETYPE_CLASS;
		else
			filetype = FILETYPE_MACHO;
	} else if ( !memcmp(buf, "\xFE\xED\xFA\xCE", 4) ) {
		filetype = FILETYPE_MACHO;
		/* ENDIAN = BIG */
		if (rad)
			printf("e cfg.bigendian = big\n");
	} else if ( !memcmp(buf, "CSR-", 4) ) {
		filetype = FILETYPE_CSRFW;
		//	config_set("asm.arch", "csr");
	} else if ( !memcmp(buf, "dex\n009\0", 8) ) {
		filetype = FILETYPE_DEX;
	} else if ( !memcmp(buf, "\x7F\x45\x4c\x46", 4) ) {
		filetype = FILETYPE_ELF;
		if (buf[EI_CLASS] == ELFCLASS64)
			elf64 = 1;
	} else if ( !memcmp(buf, "\x4d\x5a", 2) ) {
		int pe = buf[0x3c] + (buf[0x3d] << 8);
		filetype = FILETYPE_MZ;
		if (buf[pe]=='P' && buf[pe+1]=='E') {
			filetype = FILETYPE_PE;
			pebase = pe;
		}
	} else if (buf[2]==0 && buf[3]==0xea) {
		filetype = FILETYPE_ARMFW;
	} else if (buf_is_bf(buf, 4)) {
		filetype = FILETYPE_BF;
	} else {
		if (!rad)
			printf("Unknown filetype\n");
	}
	return filetype;
}

int main(int argc, char **argv, char **envp)
{
	int c;

	while ((c = getopt(argc, argv, "cerlishL:SIvxz")) != -1)
	{
		switch( c ) {
		case 'i':
			action |= ACTION_IMPORTS;
			break;
		case 'c':
			action |= ACTION_CHECKSUM;
			break;
		case 's':
			action |= ACTION_SYMBOLS;
			//action |= ACTION_EXPORTS;
			break;
		case 'S':
			action |= ACTION_SECTIONS;
			break;
		case 'I':
			action |= ACTION_INFO;
			break;
		case 'e':
			action |= ACTION_ENTRY;
			break;
		case 'l':
			action |= ACTION_LIBS;
			break;
		case 'L':
			printf("0x%08llx %s\n", (u64) addr_for_lib(optarg), optarg);
			action |= ACTION_NOP;
			break;
		case 'r':
			rad = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'x':
			xrefs = 1;
			break;
		case 'z':
			action |= ACTION_STRINGS;
			break;
		case 'h':
#if 0
		case 'a':
			action |= ACTION_ARCH;
			break;
	/* XXX depend on sections ??? */
		case 'b':
			action |= ACTION_BASE;
			break;
		case 't':
			action |= ACTION_FILETYPE;
			break;
#endif
		default:
			return rabin_show_help();
		}
	}

	file = argv[optind];

	if (action == ACTION_UNK)
		return rabin_show_help();

	if (action != ACTION_NOP) {
		if (file == NULL)
			return rabin_show_help();
		fd = open(file, O_RDONLY);
		if (fd == -1) {
			fprintf(stderr, "Cannot open file\n");
			return 0;
		}
	} else return 0;

	dm_map_file(file, fd);
	rabin_identify_header();

	if (action&ACTION_ENTRY)
		rabin_show_entrypoint(file);
	if (action&ACTION_IMPORTS)
		rabin_show_imports(file);
	if (action&ACTION_SYMBOLS)
		rabin_show_symbols(file);
	if (action&ACTION_SECTIONS)
		rabin_show_sections(file);
	if (action&ACTION_INFO)
		rabin_show_info(file);
	if (action&ACTION_LIBS)
		rabin_show_libs(file);
	if (action&ACTION_CHECKSUM)
		rabin_show_checksum(file);
	if (action&ACTION_STRINGS)
		rabin_show_strings(file);
#if 0
	if (action&ACTION_ARCH)
		rabin_show_arch(file);
	if (action&ACTION_BASE)
		rabin_show_baseaddr(file);
	if (action&ACTION_FILETYPE)
		rabin_show_filetype();
	if (action&ACTION_EXPORTS)
		rabin_show_exports(file);
	if (action&ACTION_OTHERS)
		rabin_show_others(file);
#endif

	close(fd);

	return 0;
}
