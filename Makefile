CC=arm-linux-gnueabi-gcc
LD=arm-linux-gnueabi-ld
CONFIG_SCRIBE=-D
KERNEL_OBJDIR=../obj-arm-scribe-arm/include/generated
OPTS=-c -D__ASSEMBLY__ -D__LINUX_ARM_ARCH__=7 -isystem . -include $(KERNEL_OBJDIR)/autoconf.h -include asm/unified.h -imacros linux/scribe_uaccess.h $(CONFIG_SCRIBE)CONFIG_SCRIBE
LINK_OPTS_BEGIN=-static -T armelf_linux_eabi.x -X --hash-style=both -m armelf_linux_eabi -o a.out /usr/lib/gcc/arm-linux-gnueabi/4.4.5/crtbegin.o /usr/arm-linux-gnueabi/lib/crt1.o /usr/arm-linux-gnueabi/lib/crti.o
LINK_OPTS_END=-L/usr/arm-linux-gnueabi/lib -lc -L /usr/lib/gcc/arm-linux-gnueabi/4.4.5 -lgcc /usr/lib/gcc/arm-linux-gnueabi/4.4.5/crtend.o /usr/arm-linux-gnueabi/lib/crtn.o -lgcc_eh
CFLAGS=-g -W -Wall -isystem . $(CONFIG_SCRIBE)WANT_CONFIG_SCRIBE

a.out: csumpartialcopyuser.o copy_from_user.o copy_to_user.o clear_user.o memzero.o loadcontext.o main.o extable.o
	$(LD) $(LINK_OPTS_BEGIN) -o $@ $^ $(LINK_OPTS_END)

main.o: main.c Makefile
	$(CC) -c $(CFLAGS) -D__LINUX_ARM_ARCH__=7 -o $@ $<

extable.o: extable.c Makefile
	$(CC) -c $(CFLAGS) -D__LINUX_ARM_ARCH__=7 -o $@ $<

loadcontext.o: loadcontext.S Makefile
	$(CC) $(OPTS) -o $@ $<

csumpartialcopyuser.o: csumpartialcopyuser.S Makefile
	$(CC) $(OPTS) -o $@ $<

copy_from_user.o: copy_from_user.S Makefile
	$(CC) $(OPTS) -o $@ $<

copy_to_user.o: copy_to_user.S Makefile
	$(CC) $(OPTS) -o $@ $<

clear_user.o: clear_user.S Makefile
	$(CC) $(OPTS) -o $@ $<

memzero.o: memzero.S Makefile
	$(CC) $(OPTS) -o $@ $<

clean:
	rm -f *.o
	rm -f a.out
