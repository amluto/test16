/*
 * Wrapper to run a 16-bit raw binary on Linux/i386
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <syscall.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <asm/ldt.h>
#include <string.h>
#include <asm/unistd.h>
#include <sys/mman.h>

#include "include16/sys16.h"

static unsigned char toybox[65536] __attribute__((aligned(4096)));

#define LOAD_ADDR	0x1000

static void setup_ldt(void)
{
	const struct user_desc code16_desc = {
		.entry_number	 = 0,
		.base_addr	 = (unsigned long)toybox,
		.limit		 = 0xffff,
		.seg_32bit	 = 0,
		.contents	 = 2, /* Code, R/X */
		.read_exec_only	 = 0,
		.limit_in_pages	 = 0,
		.seg_not_present = 0,
		.useable	 = 0
	};
	const struct user_desc data16_desc = {
		.entry_number	 = 1,
		.base_addr	 = (unsigned long)toybox,
		.limit		 = 0xffff,
		.seg_32bit	 = 0,
		.contents	 = 0, /* Data, R/W */
		.read_exec_only	 = 0,
		.limit_in_pages	 = 0,
		.seg_not_present = 0,
		.useable	 = 0
	};

	syscall(SYS_modify_ldt, 1, &code16_desc, sizeof code16_desc);
	syscall(SYS_modify_ldt, 1, &data16_desc, sizeof data16_desc);
}

static void load_file(const char *file, unsigned int trampoline_addr)
{
	FILE *f = fopen(file, "rb");

	if (!f) {
		fprintf(stderr, "run16: could not open: %s\n", file);
		exit(127);
	}

	fread(toybox + LOAD_ADDR, 1, trampoline_addr - LOAD_ADDR, f);
	fclose(f);
}

static void jump16(uint32_t offset)
{
	struct far_ptr {
		uint32_t offset;
		uint16_t segment;
	} target;

	target.offset = offset;
	target.segment = 7;

	asm volatile("ljmpl *%0" : : "m" (target), "a" (LOAD_ADDR));
}

#define ALIGN_UP(x,y) (((x) + (y) - 1) & ((y) - 1))

static void run(const char *file, char **argv)
{
	unsigned int trampoline_len;
	const void *trampoline;
	unsigned char *entrypoint;
	uint32_t offset;
	struct system_struct *SYS;
	char **argp;
	uint32_t argptr, argstr;

	setup_ldt();

	/* No points for style */
	asm volatile(
		" .pushsection \".rodata\",\"a\"\n"
		" .code16\n"
		"1:\n"
		" movw $15,%%cx\n"
		" movw %%cx,%%ss\n"
		" movl %2,%%esp\n"
		" movw %%cx,%%ds\n"
		" movw %%cx,%%es\n"
		" movw %%cx,%%fs\n"
		" movw %%cx,%%gs\n"
		" calll *%%eax\n"
		" movzbl %%al,%%ebx\n"
		" movl %3,%%eax\n"
		" int $0x80\n"
		"2:\n"
		" .code32\n"
		" .popsection\n"
		" movl $1b,%0\n"
		" movl $(2b - 1b),%1\n"
		: "=r" (trampoline), "=r" (trampoline_len)
		: "i" (SYS_STRUCT_ADDR), "i" (__NR_exit));

	offset = (sizeof toybox - trampoline_len) & ~15;
	entrypoint = toybox + offset;
	memcpy(entrypoint, trampoline, trampoline_len);

	/* Set up the system structure */
	SYS = (struct system_struct *)(toybox + SYS_STRUCT_ADDR);
	SYS->seg_base = (unsigned int)toybox;
	for (argp = argv; argp; argp++)
		SYS->argc++;

	argptr = ALIGN_UP(SYS_STRUCT_ADDR + sizeof(struct system_struct), 4);
	argstr = argptr + (SYS->argc + 1)*sizeof(uint32_t);

	SYS->argv = argptr;

	for (argp = argv; argp; argp++) {
		int len = strlen(*argp)+1;

		if (argstr + len >= offset) {
			fprintf(stderr, "run16: %s: command line too long\n",
				file);
			exit(127);
		}

		*(uint32_t *)(toybox + argptr) = argstr;
		argptr += sizeof(uint32_t);
		memcpy(toybox + argstr, *argp, len);
		argstr += len;
	}

	load_file(file, offset);

	/* Catch null pointer references */
	mprotect(toybox, LOAD_ADDR, PROT_NONE);

	/* The rest is fully permissive */
	mprotect(toybox + LOAD_ADDR, sizeof toybox - LOAD_ADDR,
		 PROT_READ|PROT_WRITE|PROT_EXEC);

	jump16(offset);
	abort();		/* Does not return */
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s filename [args...]\n", argv[0]);
		return 127;
	}

	run(argv[1], argv+1);
	return 127;
}
