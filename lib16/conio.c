#include <string.h>
#include <sys16.h>

unsigned long last_iret_esp;

int write(int fd, const void *buf, unsigned int count)
{
	int rv;

	asm volatile("int $0x80\n\t"
		     "mov %%esp,%0\n\t"
		     "movzwl %%sp,%%esp"
		     : "=m" (last_iret_esp), "=a" (rv)
		     : "a" (4),	/* __NR_write */
		       "b" (1),
		       "c" (_KPTR(buf)),
		       "d" (count));

	return rv;
}

void puts(const char *s)
{
	/* XXX: should loop over this */
	write(1, s, strlen(s));
}
