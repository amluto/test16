/*
 * Wrapper to run a 16-bit raw binary on Linux/i386
 */

#define _GNU_SOURCE
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

static char toybox[65536] __attribute__((aligned(4096)));

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
		fprintf(stderr, "%s: could not open: %s\n", file);
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

	asm volatile("ljmpl *%0" : : "m" (target),
		     "a" (LOAD_ADDR), "d" (offset));
}

static void run(const char *file)
{
	unsigned int trampoline_len;
	const void *trampoline;
	unsigned char *entrypoint;
	uint32_t offset;

	setup_ldt();

	/* No points for style */
	asm volatile(
		" .pushsection \".rodata\",\"a\"\n"
		" .code16\n"
		"1:\n"
		" movw $15,%%cx\n"
		" movw %%cx,%%ss\n"
		" movl %%edx,%%esp\n"
		" movw %%cx,%%ds\n"
		" movw %%cx,%%es\n"
		" movw %%cx,%%fs\n"
		" movw %%cx,%%gs\n"
		" calll *%%eax\n"
		" movzbl %%al,%%ebx\n"
		" movl %2,%%eax\n"
		" int $0x80\n"
		"2:\n"
		" .code32\n"
		" .popsection\n"
		" movl $1b,%0\n"
		" movl $(2b - 1b),%1\n"
		: "=r" (trampoline), "=r" (trampoline_len)
		: "i" (__NR_exit));

	offset = (sizeof toybox - trampoline_len - 4) & ~15;
	entrypoint = toybox + offset;
	memcpy(entrypoint, trampoline, trampoline_len);

	/* Save the segment base address in the highest dword */
	*(uint32_t *)(toybox + sizeof toybox - 4) = (uint32_t)toybox;

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
	int i;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s filename\n", argv[0]);
		return 127;
	}

	run(argv[1]);
	return 127;
}
