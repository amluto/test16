#include <sys16.h>

extern int main(int, char **);

int __attribute__((section(".init"))) _start(void)
{
	return main(SYS->argc, SYS->argv);
}
