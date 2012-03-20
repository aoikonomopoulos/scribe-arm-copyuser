CC=arm-linux-gnueabi-gcc
OPTS=-c -DCONFIG_SCRIBE -D__ASSEMBLY__ -D__LINUX_ARM_ARCH__=7 -isystem . -include ../obj-arm-scribe-arm/include/generated/autoconf.h -include asm/unified.h

a.out: copy_from_user.o memzero.o main.o
	$(CC) -o $@ $^

main.o: main.c Makefile
	$(CC) -c -W -Wall -D__LINUX_ARM_ARCH__=7 -o $@ $<

copy_from_user.o: copy_from_user.S Makefile
	$(CC) $(OPTS) -o $@ $<

memzero.o: memzero.S Makefile
	$(CC) $(OPTS) -o $@ $<

