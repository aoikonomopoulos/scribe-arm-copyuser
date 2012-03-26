#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>

#include "extable.h"

#define __user
#define UBUF_STRING "This text is the content of the user buffer"
char ubuf[] = UBUF_STRING;

/* remember: returns the number of bytes that could not be copied */
extern unsigned long   __copy_from_user(void *to, const void __user *from, unsigned long n);
extern void loadcontext(mcontext_t *);

void
scribe_pre_uaccess(const void *data, const void __user *user_ptr,
		   size_t size, int flags)
{
	printf("%s: called with (%p, %p, %zd, %d)\n", __func__, data, user_ptr, size, flags);
}

void
scribe_post_uaccess(const void *data, const void __user *user_ptr,
		    size_t size, int flags)
{
	printf("%s: called with (%p, %p, %zd, %d)\n", __func__, data, user_ptr, size, flags);
}

void
testing(void *to, void *from, unsigned long n)
{
	scribe_pre_uaccess(to, from, n, 0);
}

void
sigsegv_handler(int signo, siginfo_t *si, void *_uctx)
{
	struct exception_table_entry *e;
	unsigned long pc;
	ucontext_t *uctx = _uctx;

	pc = uctx->uc_mcontext.arm_pc;
	(void)signo;
	if (!(e = search_exception_table((void *)pc)))
		errx(1, "Cannot handle invalid pointer dereference at addr %p (pc = %#lx)\n", si->si_addr, pc);
	uctx->uc_mcontext.arm_pc = (unsigned long)e->fixup;
	loadcontext(&uctx->uc_mcontext);
	errx(1, "Can't get here!");
}

void
setup_sighandlers(void)
{
	struct sigaction sa;

	memset(&sa, '\0', sizeof(sa));
	sa.sa_sigaction = &sigsegv_handler;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGSEGV, &sa, NULL))
		err(1, "Can't set sighandler");
}

void *
setup_partially_mapped_buffer(size_t size)
{
	char *base, *ret;
	long pagesize;

	pagesize = sysconf(_SC_PAGESIZE);
	if (MAP_FAILED == (base = mmap(NULL, 2 * pagesize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)))
		err(1, "mmap");
	if (munmap(base + pagesize, pagesize))
		err(1, "munmap");
	ret = base + pagesize - (size / 2);
	strncpy(ret, UBUF_STRING, size / 2);
	return ret;
}

int
main(void)
{
	char kbuf[1024] = "";
	char *partial_ubuf;
	unsigned long ret;

	setup_sighandlers();

	print_exception_table();

	memset(kbuf, '\0', sizeof(kbuf));
	printf("kbuf = %p, ubuf = %p\n", kbuf, ubuf);
	ret = __copy_from_user(kbuf, ubuf, sizeof(ubuf));
	printf("ret = %lu, kbuf is %s\n", ret, kbuf);
	if (ret != 0) {
		errx(3, "Expected 0, got %lu\n", ret);
	}

	memset(kbuf, '\0', sizeof(kbuf));
	partial_ubuf = setup_partially_mapped_buffer(sizeof(UBUF_STRING));
	printf("kbuf = %p, partial_ubuf = %p\n", kbuf, partial_ubuf);
	ret = __copy_from_user(kbuf, partial_ubuf, sizeof(UBUF_STRING));
	printf("ret = %lu, kbuf is %s\n", ret, kbuf);
	if (ret != sizeof(UBUF_STRING)) {
		errx(3, "Expected %zu, got %ld", sizeof(UBUF_STRING), ret);
	}
	return 0;
}
