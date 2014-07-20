#include <sys16.h>

extern void puts(const char *);

static const char *hexstr(unsigned int v)
{
	int i;
	const char hexdigits[] = "0123456789abcdef";
	static char hexbuf[9];
	char *p = hexbuf;

	for (i = 0; i < 8; i++) {
		*p++ = hexdigits[v >> 28];
		v <<= 4;
	}
	*p = '\0';

	return hexbuf;
}

static inline unsigned int esp(void)
{
	unsigned int v;
	asm volatile("movl %%esp,%0" : "=rm" (v));
	return v;
}

int main(void)
{
	unsigned int initial_esp = esp();
	unsigned int eax;

	puts("Hello, World, esp = 0x");
	puts(hexstr(initial_esp));
	puts("\n");
	puts("esp after last IRET = 0x");
	puts(hexstr(last_iret_esp));
	puts("\n");

	/* Try to provoke a crash. */
	eax = -1;
	asm volatile("int $0x80; movl (%%esp),%%eax" : "+a" (eax));
	puts("Hello, Afterworld!\n");
	return 0;
}
