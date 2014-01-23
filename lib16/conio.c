#include <string.h>
#include <sys16.h>

void puts(const char *s)
{
	int rv;

	/* XXX: should loop over this */
	asm volatile("int $0x80"
		     : "=a" (rv)
		     : "a" (4),	/* __NR_write */
		       "b" (1),
		       "c" (_KPTR(s)),
		       "d" (strlen(s)));
}
