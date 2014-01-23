CC	  = gcc
LD	  = ld
AR	  = ar
OBJCOPY   = objcopy
CFLAGS	  = -g -m32 -O2
S16FLAGS  = -g -m32 -mregparm=3 -D__ASSEMBLY__ -I include16
C16FLAGS  = -g -m32 -mregparm=3 -O2 -ffreestanding \
	-include code16gcc.h -I include16
LD16FLAGS = -m elf_i386 --section-start=.init=0x1000

LIB16S    = $(wildcard lib16/*.S)
LIB16C    = $(wildcard lib16/*.c)
LIB16O    = $(LIB16C:.c=.o) $(LIB16S:.S=.o)

TEST16S   = $(wildcard test16/*.S)
TEST16C   = $(wildcard test16/*.c)
TEST16O   = $(TEST16C:.c=.o) $(TEST16S:.S=.o)
TEST16ELF = $(TEST16O:.o=.elf)
TEST16BIN = $(TEST16O:.o=.bin)

all : run16 $(TEST16BIN)

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

.PRECIOUS: test16/%.elf
test16/%.elf: test16/%.o lib16/lib16.a
	$(LD) $(LD16FLAGS) -o $@ $^

test16/%.bin: test16/%.elf
	$(OBJCOPY) -O binary $< $@

run16: run16.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f run16 *.o lib16/*.o lib16/*.a
	rm -f test16/*.o test16/*.elf test16/*.bin

spotless: clean
	rm -f *~ */*~
