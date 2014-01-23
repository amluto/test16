#ifndef SYS16_H
#define SYS16_H

#ifdef __SYS16__
# define _PTR16(x) x
#else
# define _PTR16(x) unsigned int
#endif

struct system_struct {
	unsigned int seg_base;
	int argc;
	_PTR16(char **)argv;
};

#undef _PTR16

#define SYS_STRUCT_ADDR 0xf000

#ifdef __SYS16__
# define SYS ((struct system_struct *)SYS_STRUCT_ADDR)
# define _KPTR(x) ((unsigned int)(x) + SYS->seg_base)
#endif

#endif /* SYS16_H */
