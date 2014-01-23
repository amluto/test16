#define SEG_BASE (*(const unsigned int *)0xfffc)

void puts(const char *s)
{
	int rv;

	/* XXX: should loop over this */
	asm volatile("int $0x80"
		     : "=a" (rv)
		     : "a" (4),	/* __NR_write */
		       "b" (1),
		       "c" ((unsigned int)s + SEG_BASE),
		       "d" (strlen(s)));
}
