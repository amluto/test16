extern void puts(const char *);

static const char *hexstr(unsigned int v)
{
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
	asm volatile("movl %%esp,%0 ; movzwl %%sp,%%esp" : "=rm" (v));
	return v;
}

int main(void)
{
	puts("Hello, World, esp = %\n");
	asm("movl (%%esp),%%eax" : : : "eax");
	puts("Hello, Afterworld!\n");
	return 0;
}
