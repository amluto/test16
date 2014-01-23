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
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "include16/sys16.h"

static unsigned char toybox[65536] __attribute__((aligned(4096)));
static int toyprot[65536/4096];

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

static void jump16(uint32_t offset, uint32_t start)
{
	struct far_ptr {
		uint32_t offset;
		uint16_t segment;
	} target;

	target.offset = offset;
	target.segment = 7;

	asm volatile("ljmpl *%0" : : "m" (target), "a" (start));
}

#define ALIGN_UP(x,y) (((x) + (y) - 1) & ((y) - 1))

#if PROT_NONE != 0
# error "This code assumes PROT_NONE == 0"
#endif

static int load_elf(const char *file, uint32_t *start)
{
	int fd;
	struct stat st;
	const char *fp;		/* File data pointer */
	const Elf32_Ehdr *eh;
	const Elf32_Phdr *ph;
	int rv = -1;
	int i;

	fd = open(file, O_RDONLY);
	if (fd < 0 || fstat(fd, &st))
		goto bail;

	fp = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (fp == MAP_FAILED)
		goto bail;

	errno = ENOEXEC;	/* If no other error... */

	/* Must be long enough */
	if (st.st_size < sizeof(Elf32_Ehdr))
		goto bail;

	eh = (const Elf32_Ehdr *)fp;

	/* Must be ELF, 32-bit, littleendian, version 1 */
	if (memcmp(eh->e_ident, "\x7f" "ELF\1\1\1", 6))
		goto bail;

	if (eh->e_machine != EM_386)
		goto bail;

	if (eh->e_version != EV_CURRENT)
		goto bail;

	if (eh->e_ehsize < sizeof(Elf32_Ehdr) || eh->e_ehsize >= st.st_size)
		goto bail;

	if (eh->e_phentsize < sizeof(Elf32_Phdr))
		goto bail;

	if (!eh->e_phnum)
		goto bail;

	if (eh->e_phoff + eh->e_phentsize * eh->e_phnum > st.st_size)
		goto bail;

	if (eh->e_entry > sizeof toybox)
		goto bail;

	*start = eh->e_entry;

	ph = (const Elf32_Phdr *)(fp + eh->e_phoff);

	for (i = 0; i < eh->e_phnum; i++) {
		uint32_t addr = ph->p_paddr;
		uint32_t msize = ph->p_memsz;
		uint32_t dsize = ph->p_filesz;
		int flags, page;

		ph = (const Elf32_Phdr *)((const char *)ph +
					  i*eh->e_phentsize);

		if (msize && (ph->p_type == PT_LOAD ||
			      ph->p_type == PT_PHDR)) {
			/*
			 * This loads at p_paddr, which is arguably
			 * the correct semantics (LMA).  The SysV spec
			 * says that SysV loads at p_vaddr (and thus
			 * Linux does, too); that is, however, a major
			 * brainfuckage in the spec.
			 */
			if (msize < dsize)
				dsize = msize;

			if (addr >= sizeof toybox ||
			    addr + msize > sizeof toybox)
				goto bail;

			if (dsize) {
				if (ph->p_offset >= st.st_size ||
				    ph->p_offset + dsize > st.st_size)
					goto bail;
				memcpy(toybox + addr, fp + ph->p_offset,
				       dsize);
			}

			memset(toybox + addr + dsize, 0, msize - dsize);

			flags = ((ph->p_flags & PF_X) ? PROT_EXEC : 0) |
				((ph->p_flags & PF_W) ? PROT_WRITE : 0) |
				((ph->p_flags & PF_R) ? PROT_READ : 0);

			for (page = addr & ~0xfff; page < addr + msize;
			     page += 4096)
				toyprot[page >> 12] |= flags;
		}
	}

	rv = 0;			/* Success! */

bail:
	if (fd >= 0)
		close(fd);
	return rv;
}

static void run(const char *file, char **argv)
{
	unsigned int trampoline_len;
	const void *trampoline;
	unsigned char *entrypoint;
	uint32_t offset, start;
	struct system_struct *SYS;
	char **argp;
	uint32_t argptr, argstr;
	int i;

	/* Read the input file */
	if (load_elf(file, &start))
		goto barf;

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
	for (argp = argv; *argp; argp++)
		SYS->argc++;

	argptr = ALIGN_UP(SYS_STRUCT_ADDR + sizeof(struct system_struct), 4);
	argstr = argptr + (SYS->argc + 1)*sizeof(uint32_t);

	SYS->argv = argptr;

	for (argp = argv; *argp; argp++) {
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

	/* Set up page protection */
	toyprot[0xE] |= PROT_READ | PROT_WRITE; /* Stack page */
	toyprot[0xF] |= PROT_READ | PROT_EXEC;  /* System page & trampoline */

	for (i = 0; i < 16; i++)
		mprotect(toybox + i*4096, 4096, toyprot[i]);

	setup_ldt();

	jump16(offset, start);
	abort();		/* Does not return */
barf:
	fprintf(stderr, "run16: %s: %s\n", file, strerror(errno));
	exit(127);
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
