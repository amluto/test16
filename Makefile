CC	  = gcc
LD	  = ld
AR	  = ar
OBJCOPY   = objcopy

GCCWARN   = -Wall -Wstrict-prototypes
CFLAGS	  = -g -m32 -O2 $(GCCWARN)

# Enable this if a real -m16 switch exists
#M16       = -m16
# Otherwise...
M16       = -m32 -include code16gcc.h -fno-toplevel-reorder \
	    -fno-unit-at-a-time
X16FLAGS  = -g $(M16) -D__SYS16__ -I include16
S16FLAGS  = $(X16FLAGS) -D__ASSEMBLY__
C16FLAGS  = $(X16FLAGS) $(GCCWARN) -march=i386 -mregparm=3 \
	    -Os -ffreestanding \
	    -fno-stack-protector -mpreferred-stack-boundary=2 \
	    -fno-pic

LD16FLAGS = -m elf_i386 --section-start=.init=0x1000

LIB16S    = $(wildcard lib16/*.S)
LIB16C    = $(wildcard lib16/*.c)
LIB16O    = $(LIB16C:.c=.o) $(LIB16S:.S=.o)

TEST16S   = $(wildcard test16/*.S)
TEST16C   = $(wildcard test16/*.c)
TEST16O   = $(TEST16C:.c=.o) $(TEST16S:.S=.o)
TEST16ELF = $(TEST16O:.o=.elf)

all : run16 $(TEST16ELF)

lib16/%.o: lib16/%.S
	$(CC) $(S16FLAGS) -c -o $@ $<

lib16/%.o: lib16/%.c
	$(CC) $(C16FLAGS) -c -o $@ $<

lib16/lib16.a: $(LIB16O)
	rm -f $@
	$(AR) cq $@ $^

test16/%.o: test16/%.S
	$(CC) $(S16FLAGS) -c -o $@ $<

test16/%.o: test16/%.c
	$(CC) $(C16FLAGS) -c -o $@ $<

test16/%.elf: test16/%.o lib16/lib16.a
	$(LD) $(LD16FLAGS) -o $@ $^

run16: run16.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f run16 *.o lib16/*.o lib16/*.a
	rm -f test16/*.o test16/*.elf test16/*.bin

spotless: clean
	rm -f *~ */*~
