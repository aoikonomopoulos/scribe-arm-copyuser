CC=arm-linux-gnueabi-gcc
LD=arm-linux-gnueabi-ld
OPTS=-c -DCONFIG_SCRIBE -D__ASSEMBLY__ -D__LINUX_ARM_ARCH__=7 -isystem . -include ../obj-arm-scribe-arm/include/generated/autoconf.h -include asm/unified.h
LINK_OPTS_BEGIN=-static -T armelf_linux_eabi.x -X --hash-style=both -m armelf_linux_eabi -o a.out /usr/lib/gcc/arm-linux-gnueabi/4.4.5/crtbegin.o /usr/arm-linux-gnueabi/lib/crt1.o /usr/arm-linux-gnueabi/lib/crti.o
LINK_OPTS_END=-L/usr/arm-linux-gnueabi/lib -lc -L /usr/lib/gcc/arm-linux-gnueabi/4.4.5 -lgcc /usr/lib/gcc/arm-linux-gnueabi/4.4.5/crtend.o /usr/arm-linux-gnueabi/lib/crtn.o -lgcc_eh

a.out: copy_from_user.o memzero.o main.o
	$(LD) $(LINK_OPTS_BEGIN) -o $@ $^ $(LINK_OPTS_END)

main.o: main.c Makefile
	$(CC) -c -W -Wall -D__LINUX_ARM_ARCH__=7 -o $@ $<

copy_from_user.o: copy_from_user.S Makefile
	$(CC) $(OPTS) -o $@ $<

memzero.o: memzero.S Makefile
	$(CC) $(OPTS) -o $@ $<

