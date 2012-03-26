#include <err.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>

#include "extable.h"

#define __user
#define UBUF_STRING "This text is the content of the user buffer"

/* remember: returns the number of bytes that could not be copied */
extern unsigned long   __copy_from_user(void *to, const void __user *from, unsigned long n);
extern unsigned long   __copy_to_user(void __user *to, const void *from, unsigned long n);

extern void loadcontext(mcontext_t *);

struct scribe_uaccess_log {
	bool pre;
	int pre_flags;
	bool post;
	int post_flags;
} scribe_uaccess_log;

static void
scribe_uaccess_init(void)
{
	scribe_uaccess_log = (struct scribe_uaccess_log){
		.pre = false,
		.pre_flags = -1,
		.post = false,
		.post_flags = -1,
	};
}

#define bool_str(v) ((v) ? "true" : "false")
#define assert_eq(fmt, a, b)			\
	do {					\
		typeof(a) __a = (a);			\
		typeof(b) __b = (b);			\
		if (__a != __b) {			\
			fprintf(stderr, "%s != %s (", #a, #b);	\
			fprintf(stderr, fmt, __a, __b);			\
			fprintf(stderr, ")\n");				\
			abort();					\
		}							\
	} while (0)
static void
scribe_uaccess_verify(bool pre, int pre_flags, bool post, int post_flags)
{
	assert_eq("%d != %d", scribe_uaccess_log.pre, pre);
	assert_eq("%d != %d",scribe_uaccess_log.pre_flags, pre_flags);

	assert_eq("%d != %d", scribe_uaccess_log.post, post);
	assert_eq("%d != %d",scribe_uaccess_log.post_flags, post_flags);
}

void
scribe_pre_uaccess(const void *data, const void __user *user_ptr,
		   size_t size, int flags)
{
	scribe_uaccess_log.pre = true;
	scribe_uaccess_log.pre_flags = flags;
	printf("%s: called with (%p, %p, %zd, %d)\n", __func__, data, user_ptr, size, flags);
}

void
scribe_post_uaccess(const void *data, const void __user *user_ptr,
		    size_t size, int flags)
{
	scribe_uaccess_log.post = true;
	scribe_uaccess_log.post_flags = flags;
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

static void
test___copy_from_user(void)
{
	char ubuf[] = UBUF_STRING;
	char kbuf[1024] = "";
	char *partial_ubuf;
	unsigned long ret;

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
	scribe_uaccess_verify(true, 0, true, 0);
}

static void
test___copy_to_user(void)
{
	char kbuf[] = UBUF_STRING;
	char ubuf[1024];
	char *partial_ubuf;
	unsigned long ret;

	printf("kbuf = %p, ubuf = %p\n", kbuf, ubuf);
	ret = __copy_to_user(ubuf, kbuf, sizeof(kbuf));
	if (ret != 0)
		errx(3, "Expected %zu, got %ld", 0, ret);
	printf("ret = %lu, ubuf is %s\n", ret, ubuf);
	partial_ubuf = setup_partially_mapped_buffer(sizeof(kbuf));
	printf("kbuf = %p, partial_ubuf = %p\n", kbuf, partial_ubuf);
	ret = __copy_to_user(partial_ubuf, kbuf, sizeof(kbuf));
	printf("ret = %lu, partial_ubuf is %.*s\n", ret, sizeof(kbuf) / 2, partial_ubuf);
	if (ret != (sizeof(kbuf) / 2)) {
		errx(3, "Expected %zu, got %ld", (sizeof(kbuf) / 2), ret);
	}
	scribe_uaccess_verify(true, 0, true, 0);
}

static struct testcase {
	const char *name;
	void (*func)(void);
} testcases[] = {
#define TC(kfunc)					\
	{						\
		.name = #kfunc,				\
		.func = test ## _ ## kfunc,		\
	},
	TC(__copy_from_user)
	TC(__copy_to_user)
#undef TC
};

static void
testcases_list(void)
{
	unsigned i;
	for (i = 0; i < (sizeof(testcases) / sizeof(testcases[0])); ++i) {
		printf("%s\n", testcases[i].name);
	}
}

int
main(int argc, char **argv)
{
	unsigned i;
	const char *testcase;

	if (argc != 2)
		errx(2, "need one argument\n");
	if (!strcmp("list", argv[1])) {
		testcases_list();
		exit(0);
	}
	setup_sighandlers();
	print_exception_table();

	testcase = argv[1];
	for (i = 0; i < (sizeof(testcases) / sizeof(testcases[0])); ++i) {
		if (strcmp(testcases[i].name, testcase))
			continue;
		printf("*** %s\n", testcases[i].name);
		scribe_uaccess_init();
		testcases[i].func();
		exit(0);
	}
	errx(2, "no such testcase: %s\n", testcase);
	return 0;
}
