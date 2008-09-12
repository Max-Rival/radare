/*
 * Copyright (C) 2006, 2007, 2008
 *       pancake <youterm.com>
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

#include "code.h"
#include "main.h"
#include "code.h"
#include "radare.h"
#include "utils.h"
#include "config.h"
#include "list.h"
#include "readline.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct config_t config;

int rdb_init()
{
	const char *rdbdir;
	const char *rdbfile;
	int fd = -1;
	char *str =
		"# RDB (radare database) file\n"
		"chdir=\nchroot=\nsetuid=\nsetgid=\n";
/* FUCKY! */
// just set project name (cfg.project)

	rdbfile = config_get("file.project");
	if (strnull(rdbfile))
		return -1;

	fd = open(rdbfile, O_APPEND|O_RDWR, 0644);
	rdbdir = config_get("dir.project");
	if (rdbdir&&rdbdir[0]) {
		mkdir(rdbdir, 0755);
		chdir(rdbdir);
	}
	if (fd == -1) {
		fd = open(rdbfile, O_CREAT|O_APPEND|O_RDWR, 0644);
		if (fd != -1 )
			write(fd, (const void *)str, strlen(str));
		else {
			eprintf("Cannot open '%s' for writting.\n", rdbfile);
			return -1;
		}
	}
	close(fd);

	return open(rdbfile, O_CREAT|O_APPEND|O_RDWR, 0644);
}

static void config_old_init()
{
	memset(&config, '\0', sizeof(config));
	config.mode        = MODE_SHELL;
	config.endian      = 1;
	config.noscript    = 0;
	config.script      = NULL;
	config.baddr       = 0;
	config.seek        = 0;
	config.arch        = ARCH_X86;
	config.lines       = 0;
	config.debug       = 0;
	config.color	   = 0;
	config.unksize     = 0;
	config.buf         = 0; // output buffered
	config.ene         = 10;
	config.width       = cons_get_columns();
	config.last_seek   = 0;
	config.file        = NULL;
	config.block_size  = DEFAULT_BLOCK_SIZE;
	config.cursor      = 0;
	config.ocursor     = -1;
	config.block       = (unsigned char *)malloc(config.block_size);
	config.verbose     = 1;
	config.interrupted = 1;
	config.visual      = 0;
	config.lang        = 0;
	config.fd          = -1;
	config.zoom.size   = config.size;
	config.zoom.from   = 0;
	config.zoom.enabled= 0;
	config.zoom.piece  = config.size/config.block_size;
#if HAVE_PERL
	config.lang        = LANG_PERL;
#endif
#if HAVE_PYTHON
	config.lang        = LANG_PYTHON;
#endif
	INIT_LIST_HEAD(&config.rdbs);
}

/* new stuff : radare config 2.0 */

struct config_new_t config_new;

struct config_node_t* config_node_new(const char *name, const char *value)
{
	struct config_node_t *node = 
		(struct config_node_t *)malloc(sizeof(struct config_node_t));

	INIT_LIST_HEAD(&(node->list));
	node->name = strdup(name);
	node->hash = strhash(name);
	node->value = value?strdup(value):strdup("");
	node->flags = CN_RW | CN_STR;
	node->i_value = 0;
	node->callback = NULL;

	return node;
}

void config_list(char *str)
{
	struct list_head *i;
	int len = 0;

	if (str&&str[0]) {
		str = strclean(str);
		len = strlen(str);
	}

	list_for_each(i, &(config_new.nodes)) {
		struct config_node_t *bt = list_entry(i, struct config_node_t, list);
		if (str) {
			if (strncmp(str, bt->name,len) == 0)
				cons_printf("%s = %s\n", bt->name, bt->value);
		} else {
			cons_printf("%s = %s\n", bt->name, bt->value);
		}
	}
}

struct config_node_t *config_node_get(const char *name)
{
	struct list_head *i;
	int hash = strhash(name);

	list_for_each_prev(i, &(config_new.nodes)) {
		struct config_node_t *bt = list_entry(i, struct config_node_t, list);
		if (bt->hash == hash)
			return bt;
	}

	return NULL;
}

const char *config_get(const char *name)
{
	struct config_node_t *node;

	node = config_node_get(name);
	if (node) {
		if (node->flags & CN_BOOL)
			return (const char *)
				(((!strcmp("true", node->value))
				 || (!strcmp("1", node->value)))?(const char *)1:NULL);
		return node->value;
	}

	return NULL;
}

u64 config_get_i(const char *name)
{
	struct config_node_t *node;

	node = config_node_get(name);
	if (node) {
		if (node->i_value != 0)
			return node->i_value;
		return (u64)get_math(node->value);
	}

	return (u64)0LL;
}

struct config_node_t *config_set(const char *name, const char *value)
{
	struct config_node_t *node;

	if (name[0] == '\0')
		return NULL;

	node = config_node_get(name);

	// TODO: store old value anywhere or something..
	if (node) {
		if (node->flags & CN_RO) {
			eprintf("(read only)\n");
			return node;
		}
		free(node->value);
		if (node->flags & CN_BOOL) {
			int b = (!strcmp(value,"true")||!strcmp(value,"1"));
			node->i_value = (u64)b;
			//node->value = estrdup(node->value, b?"true":"false");
			node->value = strdup(b?"true":"false");
		} else {
			if (value == NULL) {
				node->value = strdup("");
				node->i_value = 0;
			} else {
				node->value = strdup(value);
				if (strchr(value, '/'))
					node->i_value = get_offset(value);
				else	node->i_value = get_math(value);
				node->flags |= CN_INT;
			}
		}
	} else {
		if (config_new.lock) {
			eprintf("(config-locked: '%s' no new keys can be created)\n", name);
		} else {
			node = config_node_new(name, value);
			if (value)
			if (!strcmp(value,"true")||!strcmp(value,"false"))
				node->flags|=CN_BOOL;
			list_add_tail(&(node->list), &(config_new.nodes));
		}
	}

	if (node&&node->callback)
	 	node->callback(node);

	return node;
}

int config_rm(const char *name)
{
	struct config_node_t *node;

	node = config_node_get(name);

	if (node)
		cons_printf("TODO: remove: not yet implemented\n");
	else
		cons_printf("node not found\n");

	return 0;
}

struct config_node_t *config_set_i(const char *name, const u64 i)
{
	char buf[128];
	struct config_node_t *node;

	node = config_node_get(name);

	if (node) {
		if (node->flags & CN_RO)
			return NULL;
		free(node->value);
		sprintf(buf, "%lld", i); //0x%08lx", i);
		node->value = strdup(buf);
		node->flags = CN_RW | CN_INT;
		node->i_value = i;
	} else {
		if (config_new.lock) {
			eprintf("(locked: no new keys can be created)");
		} else {
			sprintf(buf, "%d", (unsigned int)i);//OFF_FMTd, (u64) i);
			node = config_node_new(name, buf);
			node->flags = CN_RW | CN_OFFT;
			node->i_value = i;
			list_add_tail(&(node->list), &(config_new.nodes));
		}
	}

	if (node&&node->callback)
	 	node->callback(node);

	return node;
}

void config_eval(char *str)
{
	char *ptr,*a,*b;
	char *name;

	if (str == NULL)
		return;
	name = strdup(str);

	str = strclean(name);

	if (str && (str[0]=='\0'||!strcmp(str, "help"))) {
		config_list(NULL);
		return;
	}

	if (str[0]=='-') {
		config_rm(str+1);
		return;
	}

	ptr = strchr(str, '=');
	if (ptr) {
		/* set */
		ptr[0]='\0';
		a = strclean(name);
		b = strclean(ptr+1);
		config_set(a, b);
	} else {
		char *foo = strclean(name);
		if (foo[strlen(foo)-1]=='.') {
			/* list */
			config_list(name);
		} else {
			/* get */
			const char * str = config_get(foo);
			cons_printf("%s\n", (((int)str)==1)?"true":(str==0)?"false":str);
		}
	}

	free(name);
}

static int config_zoombyte_callback(void *data)
{
	struct config_node_t *node = data;

	if (!strcmp(node->value, "head")) {
		// ok
	} else
	if (!strcmp(node->value, "flags")) {
		// ok
	} else
	if (!strcmp(node->value, "FF")) {
		// ok
	} else
	if (!strcmp(node->value, "entropy")) {
		// ok
	} else
	if (!strcmp(node->value, "print")) {
		// ok
	} else
	if (!strcmp(node->value, "printable")) {
		// ok
	} else {
		free(node->value);
		node->value = strdup("head");
		return 0;
	}
	return 1;
}

#if 0
static int config_core_callback(void *data)
{
	struct config_node_t *node = data;

	if (!strcmp(node->name, "core.jmp")) {
		hist_goto(node->value);
	} else
	if (!strcmp(node->name, "core.echo")) {
		cons_printf("%s\n", node->value);
	} else
	if (!strcmp(node->name, "core.cmp")) {
		hist_cmp(node->value);
	} else
	if (!strcmp(node->name, "core.load")) {
		hist_load(node->value);
	} else
	if (!strcmp(node->name, "core.dump")) {
		hist_dump(node->value);
	} else
	if (!strcmp(node->name, "core.list")) {
		hist_show();
	} else
	if (!strcmp(node->name, "core.reset")) {
		hist_reset();
	} else
	if (!strcmp(node->name, "core.je")) {
		hist_cgoto(node->value, OP_JE);
	} else
	if (!strcmp(node->name, "core.jne")) {
		hist_cgoto(node->value, OP_JNE);
	} else
	if (!strcmp(node->name, "core.ja")) {
		hist_cgoto(node->value, OP_JA);
	} else
	if (!strcmp(node->name, "core.jb")) {
		hist_cgoto(node->value, OP_JB);
	} else
	if (!strcmp(node->name, "core.loop")) {
		hist_loop(node->value);
	} else
	if (!strcmp(node->name, "core.break")) {
		// ignored
		//hist_break();
	} else
	if (!strcmp(node->name, "core.label")) {
		hist_add_label(node->value);
	}
	// TODO needs more work
}
#endif

static int config_limit_callback(void *data)
{
	struct config_node_t *node = data;

	config.limit = get_offset(node->value);

	return 0;
}

static int config_arch_callback(void *data)
{
	radis_update();

	return 0;
}

static int config_verbose_callback(void *data)
{
	struct config_node_t *node = data;

	if (node && node->i_value) {
		config.verbose = node->i_value;
		dl_echo = config.verbose;
	}

	return 0;
}

static int config_wmode_callback(void *data)
{
	//struct config_node_t *node = data;
	//if (node && node->i_value)
	// XXX: strange magic conditional
	if (config.fd != -1 && config.file && !config.debug) // && config_new.lock)
		radare_open(0);

	return 1;
}

static int config_palette_callback(void *data)
{
	struct config_node_t *node = data;

	cons_palette_init(node->value);

	return 1;
}

static int config_color_callback(void *data)
{
	struct config_node_t *node = data;

	if (node && node->i_value)
		config.color = (int)node->i_value;
	return 1;
}

#if 0
int config_asm_dwarf(void *data)
{
	struct config_node_t *node = data;

	if (node && node->value)
		config_set("cmd.asm", "!rsc dwarf-addr ${FILE} ${HERE}");
	else	config_set("cmd.asm", "");
	return 1;
}
#endif

static int config_baddr_callback(void *data)
{
	struct config_node_t *node = data;

	if (node)
		config.baddr = (u64)node->i_value;
	return 1;
}

static int config_scrheight(void *data)
{
	struct config_node_t *node = data;
	config.height = node->i_value;
	if (config.height<1) {
		cons_get_real_columns();
		if (config.height<1)
			config.height = 24;
	}
	return config.height;
}

static int config_scrbuf_callback(void *data)
{
	struct config_node_t *node = data;

	config.buf = node->i_value;
	return 1;
}

static int config_bsize_callback(void *data)
{
	struct config_node_t *node = data;

	if (node->i_value)
		radare_set_block_size_i(node->i_value);
	return 1;
}

void config_lock(int l)
{
	config_new.lock = l;
}

void config_init(int first)
{
	char buf[1024];
	const char *ptr;
	struct config_node_t *node;

	if (first) {
		flag_init();
		config_old_init();

		// TODO : dl_callback = radare_dl_autocompletion;
		dl_init();
		dl_hist_load(".radare_history");

		config_new.n_nodes = 0;
		config_new.lock = 0;
		INIT_LIST_HEAD(&(config_new.nodes));

	}

	vm_init(1);

	/* enter keys */
#if __POWERPC__
        node = config_set("asm.arch", "ppc");
#elif __x86_64__
	node = config_set("asm.arch", "intel64");
#elif __arm__
	node = config_set("asm.arch", "arm");
#elif __mips__
	node = config_set("asm.arch", "mips");
#else
	node = config_set("asm.arch", "intel");
#endif
	node->callback = &config_arch_callback;
	config_set("asm.comments", "true"); // show comments in disassembly
	config_set_i("asm.cmtmargin", 27); // show comments in disassembly
	config_set_i("asm.cmtlines", 0); // show comments in disassembly
	config_set("asm.syntax", "pseudo");
	config_set("asm.objdump", "objdump -m i386 --target=binary -D");
	config_set("asm.offset", "true"); // show offset
	config_set("asm.reladdr", "false"); // relative offset
	config_set_i("asm.nbytes", 8); // show hex bytes
	config_set("asm.bytes", "true"); // show hex bytes
	config_set("asm.flags", "true"); // show hex bytes
	config_set("asm.flagsline", "false"); // show hex bytes
	config_set("asm.functions", "true"); // show hex bytes
	config_set("asm.lines", "true"); // show left ref lines
	config_set_i("asm.nlines", 6); // show left ref lines
	config_set("asm.lineswide", "true"); // show left ref lines
	config_set("asm.trace", "false"); // trace counter
	config_set("asm.linesout", "false"); // show left ref lines
	config_set("asm.linestyle", "false"); // foreach / prev
	// asm.os = used for syscall tables and so.. redefined with rabin -rI
#if __linux__
	config_set("asm.os", "linux"); 
#elif __FreeBSD__
	config_set("asm.os", "freebsd");
#elif __NetBSD__
	config_set("asm.os", "netbsd");
#elif __OpenBSD__
	config_set("asm.os", "openbsd");
#elif __Windows__
	config_set("asm.os", "linux");
#endif
	config_set("asm.split", "true"); // split code blocks
	config_set("asm.splitall", "false"); // split code blocks
	config_set("asm.size", "false"); // opcode size
	config_set("asm.xrefs", "xrefs");

	config_set("asm.follow", "");
	config_set("cmd.wp", "");
	config_set("cmd.flag", "true");
	config_set("cmd.asm", "");
	config_set("cmd.user", "");
	config_set("cmd.visual", "");
	config_set("cmd.hit", "");
	config_set("cmd.prompt", "");
	config_set("cmd.vprompt", "p%");
	config_set("cmd.bp", "");

	config_set("search.flag", "true");
	config_set("search.verbose", "true");

	config_set("file.id", "false");
	config_set("file.type", "");
	config_set("file.flag", "false");
	config_set("file.trace", "trace.log");
	config_set("file.project", "");
	config_set("file.entrypoint", "");
	config_set("file.scrfilter", "");
	config_set_i("file.size", 0);
	node = config_set_i("file.baddr", 0);
	node->callback = &config_baddr_callback;

	config_set("trace.bt", "false");
	config_set_i("trace.sleep", 0);
	config_set("trace.smart", "false");
	config_set("trace.libs", "true");
	config_set("trace.log", "false");
	config_set("trace.dup", "false");
	config_set("trace.cmtregs", "false");

	config_set("cfg.editor", "vi");
	config_set("cfg.noscript", "false");
	config_set("cfg.encoding", "ascii"); // cp850
	config_set_i("cfg.delta", 1024); // cp850
	node = config_set("cfg.verbose", "true");
	node->callback = &config_verbose_callback;
#if LIL_ENDIAN
	config_set("cfg.bigendian", "false");
#else
	config_set("cfg.bigendian", "true");
#endif
	config_set("cfg.inverse", "false");
	config_set_i("cfg.analdepth", 6);
	config_set("file.insert", "false");
	config_set("file.insertblock", "false");
	if (first) {
		node = config_set("file.write", "false");
		node->callback = &config_wmode_callback;
	}
	node = config_set("cfg.limit", "0");
	node->callback = &config_limit_callback;
#if __mips__
	// ???
	config_set("cfg.addrmod", "32");
#else
	config_set("cfg.addrmod", "4");
#endif
	config_set("cfg.rdbdir", "TODO");
	config_set("cfg.datefmt", "%d:%m:%Y %H:%M:%S %z");
	config_set_i("cfg.count", 0);
	config_set("cfg.fortunes", "true");
	node = config_set_i("cfg.bsize", 512);
	node->callback = &config_bsize_callback;
	config_set_i("cfg.vbsize", 1024);
	config_set("cfg.vbsize_enabled", "false");

	config_set("child.stdio", "");
	config_set("child.stdin", "");
	config_set("child.stdout", "");
	config_set("child.stderr", "");
	config_set("child.setgid", ""); // must be int ?
	config_set("child.chdir", ".");
	config_set("child.chroot", "/");
	config_set("child.setuid", "");
#if __mips__
	config_set("dbg.fpregs", "true");
#else
	config_set("dbg.fpregs", "false");
#endif
	config_set("dbg.forks", "false"); // stop debugger in any fork or clone
	config_set("dbg.controlc", "true"); // stop debugger if ^C is pressed
	config_set("dbg.syms", "true");
	config_set("dbg.dwarf", "false");
	config_set("dbg.maps", "true");
	config_set("dbg.sections", "true");
	config_set("dbg.strings", "false");
	config_set("dbg.stop", "false");
	config_set("dbg.threads", "false");
	config_set("dbg.contscbt", "true");
	config_set("dbg.regs", "true");
	config_set("dbg.stack", "true");
	config_set("dbg.vstack", "true");
	config_set("dbg.wptrace", "false");
	config_set_i("dbg.stacksize", 66);
	config_set("dbg.stackreg", "esp");
	config_set("dbg.bt", "false");
	config_set("dbg.fullbt", "false"); // user backtrace or lib+user backtrace
	config_set("dbg.bttype", "default"); // default, st and orig or so!
#if __APPLE__ || __ARM__ || __mips__
	config_set("dbg.hwbp", "false"); // default, st and orig or so!
#else
	config_set("dbg.hwbp", "true"); // hardware breakpoints by default // ALSO BSD
#endif
	config_set("dbg.bep", "loader"); // loader, main
	config_set("dir.home", getenv("HOME"));

	/* dir.monitor */
	ptr = getenv("MONITORPATH");
	if (ptr == NULL) {
		sprintf(buf, "%s/.radare/monitor/", getenv("HOME"));
		ptr = (const char *)&buf;
	}
	config_set("dir.monitor", ptr);

	/* dir.spcc */
	ptr = getenv("SPCCPATH");
	if (ptr == NULL) {
		sprintf(buf, "%s/.radare/spcc/", getenv("HOME"));
		ptr = buf;
	}
	config_set("dir.spcc", ptr);

	config_set("dir.plugins", LIBDIR"/radare/");
	snprintf(buf, 1023, "%s/.radare/rdb/", getenv("HOME"));
	config_set("dir.project", buf); // ~/.radare/rdb/
	config_set("dir.tmp", get_tmp_dir());
	config_set("graph.color", "magic");
	config_set("graph.split", "false"); // split blocks // SHOULD BE TRUE, but true algo is buggy
	config_set("graph.jmpblocks", "true");
	config_set("graph.refblocks", "false"); // must be circle nodes
	config_set("graph.callblocks", "false");
	config_set("graph.flagblocks", "true");
	config_set_i("graph.depth", 9);
	config_set("graph.offset", "false");
	config_set("graph.render", "cairo"); // aalib/ncurses/text

	node = config_set_i("zoom.from", 0);
	node = config_set_i("zoom.size", config.size);
	node = config_set("zoom.byte", "head");
	node->callback = &config_zoombyte_callback;

	config_set("scr.html", "false");
	config_set_i("scr.accel", 0);
	node = config_set("scr.palette", cons_palette_default);
	node->callback = &config_palette_callback;
	config_set("scr.seek", "");
	node = config_set("scr.color", (config.color)?"true":"false");
	node->callback = &config_color_callback;
	config_set("scr.tee", "");
	node = config_set("scr.buf", "false");
	node->callback = &config_scrbuf_callback;
	config_set_i("scr.width", config.width);
	node = config_set_i("scr.height", config.height);
	node->callback = &config_scrheight;
#if 0

	/* core commands */
	node = config_set("core.echo", "(echo a message)");
	node->callback = &config_core_callback;
	node = config_set("core.jmp", "(jump to label)");
	node->callback = &config_core_callback;
	node = config_set("core.je", "(conditional jump)");
	node->callback = &config_core_callback;
	node = config_set("core.jne", "(conditional jump)");
	node->callback = &config_core_callback;
	node = config_set("core.ja", "(conditional jump)");
	node->callback = &config_core_callback;
	node = config_set("core.jb", "(conditional jump)");
	node->callback = &config_core_callback;
	node = config_set("core.cmp", "(compares two comma separated flags)");
	node->callback = &config_core_callback;
	node = config_set("core.loop", "(loop n times to label (core.loop = 3,foo))");
	node->callback = &config_core_callback;
	node = config_set("core.label", "(create a new label)");
	node->callback = &config_core_callback;
	node = config_set("core.break", "(breaks a loop)");
	node->callback = &config_core_callback;
	node = config_set("core.list", "(list the code)");
	node->callback = &config_core_callback;
	node = config_set("core.load", "(loads code from a file)");
	node->callback = &config_core_callback;
	node = config_set("core.dump", "(dumps the to a file)");
	node->callback = &config_core_callback;
	node = config_set("core.reset", "(resets code)");
	node->callback = &config_core_callback;
#endif

	/* lock */
	config_lock(1);

	cons_palette_init(config_get("scr.palette"));
	radis_update();

	if (first) {
		metadata_comment_init(1);
		radare_hack_init();
		trace_init();
	}
}

void config_visual_hit_i(const char *name, int delta)
{
	char buf[1024];
	struct config_node_t *node;
	node = config_node_get(name);
	if (node) {
		if (node->flags & CN_INT || node->flags & CN_OFFT) {
			config_set_i(name, config_get_i(name)+delta);
		}
	}
}

/* Visually activate the config variable */
void config_visual_hit(const char *name)
{
	char buf[1024];
	struct config_node_t *node;
	node = config_node_get(name);
	if (node) {
		if (node->flags & CN_BOOL) {
			/* TOGGLE */
			node->i_value = !node->i_value;
			node->value = estrdup(node->value, node->i_value?"true":"false");
		} else {
			// FGETS AND SO
			cons_printf("New value (old=%s): ", node->value);
			cons_flush();
			cons_set_raw(0);
			cons_fgets(buf, 1023, 0, 0);
			cons_set_raw(1);
			node->value = estrdup(node->value, buf);
		}
	}
}

/* Like emenu but for real */
void config_visual_menu()
{
	char cmd[1024];
	struct list_head *pos;
#define MAX_FORMAT 2
	int format = 0;
	const char *ptr;
	char *fs = NULL;
	char *fs2 = NULL;
	int option = 0;
	int _option = 0;
	int delta = 9;
	int menu = 0;
	int i,j, ch;
	int hit;
	int show;
	char old[1024];
	old[0]='\0';

	while(1) {
		cons_gotoxy(0,0);
		cons_clear();

		/* Execute visual prompt */
		ptr = config_get("cmd.vprompt");
		if (ptr&&ptr[0]) {
			int tmp = last_print_format;
			radare_cmd_raw(ptr, 0);
			last_print_format = tmp;
		}

		switch(menu) {
		case 0: // flag space
			cons_printf("\n Eval spaces:\n\n");
			hit = 0;
			j = i = 0;
			list_for_each(pos, &(config_new.nodes)) {
				struct config_node_t *bt = list_entry(pos, struct config_node_t, list);
				if (option==i) {
					fs = bt->name;
					hit = 1;
				}
				show = 0;
				if (old[0]=='\0') {
					strccpy(old, bt->name, '.');
					show = 1;
				} else if (strccmp(old, bt->name, '.')) {
					strccpy(old, bt->name, '.');
					show = 1;
				}

				if (show) {
					if( (i >=option-delta) && ((i<option+delta)||((option<delta)&&(i<(delta<<1))))) {
						cons_printf(" %c  %s\n", (option==i)?'>':' ', old);
						j++;
					}
					i++;
				}
			}
			if (!hit && j>0) {
				option = j-1;
				continue;
			}
			cons_printf("\n Sel:%s \n\n", fs);
			break;
		case 1: // flag selection
			cons_printf("\n Eval variables: (%s)\n\n", fs);
			hit = 0;
			j = i = 0;
			// TODO: cut -d '.' -f 1 | sort | uniq !!!
			list_for_each(pos, &(config_new.nodes)) {
				struct config_node_t *bt = list_entry(pos, struct config_node_t, list);
				if (option==i) {
					fs2 = bt->name;
					hit = 1;
				}
				if (!strccmp(bt->name, fs, '.')) {
					if( (i >=option-delta) && ((i<option+delta)||((option<delta)&&(i<(delta<<1))))) {
					// TODO: Better align
						cons_printf(" %c  %s = %s\n", (option==i)?'>':' ', bt->name, bt->value);
						j++;
					}
				i++;
				}
			}
			if (!hit && j>0) {
				option = i-1;
				continue;
			}
			cons_printf("\n Selected: %s\n\n", fs2);
		}
		cons_flush();
		ch = cons_readchar();
		ch = cons_get_arrow(ch); // get ESC+char, return 'hjkl' char
		switch(ch) {
		case 'j':
			option++;
			break;
		case 'k':
			if (--option<0)
				option = 0;
			break;
		case 'h':
		case 'b': // back
			menu = 0;
			option = _option;
			break;
		case 'q':
			if (menu<=0) return; menu--;
			break;
		case '*':
		case '+':
			config_visual_hit_i(fs2, +1);
			continue;
		case '/':
		case '-':
			config_visual_hit_i(fs2, -1);
			continue;
		case 'l':
		case 'e': // edit value
		case ' ':
		case '\r':
		case '\n': // never happens
			if (menu == 1) {
				config_visual_hit(fs2);
			} else {
				flag_space_set(fs);
				menu = 1;
				_option = option;
				option = 0;
			}
			break;
		case '?':
			cons_clear00();
			cons_printf("\nVe: Visual Eval help:\n\n");
			cons_printf(" q     - quit menu\n");
			cons_printf(" j/k   - down/up keys\n");
			cons_printf(" h/b   - go back\n");
			cons_printf(" e/' ' - edit/toggle current variable\n");
			cons_printf(" +/-   - increase/decrease numeric value\n");
			cons_printf(" :     - enter command\n");
			cons_flush();
			cons_any_key();
			break;
		case ':':
			cons_set_raw(0);
#if HAVE_LIB_READLINE
			char *ptr = readline(VISUAL_PROMPT);
			if (ptr) {
				strncpy(cmd, ptr, sizeof(cmd));
				radare_cmd(cmd, 1);
				free(ptr);
			}
#else
			cmd[0]='\0';
			dl_prompt = ":> ";
			if (cons_fgets(cmd, 1000, 0, NULL) <0)
				cmd[0]='\0';
			//line[strlen(line)-1]='\0';
			radare_cmd(cmd, 1);
#endif
			cons_set_raw(1);
			if (cmd[0])
				cons_any_key();
			cons_gotoxy(0,0);
			cons_clear();
			continue;
		}
	}
}
