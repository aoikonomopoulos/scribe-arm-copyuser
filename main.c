#include <err.h>
#include <errno.h>
#include <linux/scribe_uaccess.h>
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
unsigned int csum_partial_copy_from_user(const char *src, char *dst, int len, int sum, int *err_ptr);

/* returns number of bytes not cleared */
int __clear_user(void *addr, size_t sz);

extern void loadcontext(mcontext_t *);

struct scribe_uaccess_log {
	bool done;
	int flags;
	const void *data;
	const void *uptr;
	size_t size;
} scribe_uaccess_log_pre, scribe_uaccess_log_post;

static void
scribe_uaccess_init_log(struct scribe_uaccess_log *log)
{
	*log = (struct scribe_uaccess_log){
		.done = false,
		.data = (void *)0xdeadbeef,
		.uptr = (void *)0xdeadbeef,
		.size = (size_t)-1,
		.flags = -1,
	};
}

static void
scribe_uaccess_init(void)
{
	scribe_uaccess_init_log(&scribe_uaccess_log_pre);
	scribe_uaccess_init_log(&scribe_uaccess_log_post);
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
scribe_uaccess_verify(struct scribe_uaccess_log *log, bool done, void *data, void *uptr, size_t size, int flags)
{
#ifndef WANT_CONFIG_SCRIBE
	assert_eq("%d != %d", log->done, false);
	return;
#endif
	assert_eq("%d != %d", log->done, done);
	if (!done)
		return;
	assert_eq("%p != %p", log->data, data);
	assert_eq("%p != %p", log->uptr, uptr);
	assert_eq("%zd != %zd", log->size, size);
	assert_eq("%d != %d", log->flags, flags);
}

void
scribe_pre_uaccess(const void *data, const void __user *user_ptr,
		   size_t size, int flags)
{
	scribe_uaccess_log_pre.done = true;
	scribe_uaccess_log_pre.data = data;
	scribe_uaccess_log_pre.uptr = user_ptr;
	scribe_uaccess_log_pre.flags = flags;
	scribe_uaccess_log_pre.size = size;
	printf("%s:\tcalled with (%p, %p, %zd, %d)\n", __func__, data, user_ptr, size, flags);
}

void
scribe_post_uaccess(const void *data, const void __user *user_ptr,
		    size_t size, int flags)
{
	scribe_uaccess_log_post.done = true;
	scribe_uaccess_log_post.data = data;
	scribe_uaccess_log_post.uptr = user_ptr;
	scribe_uaccess_log_post.flags = flags;
	scribe_uaccess_log_post.size = size;
	printf("%s:\tcalled with (%p, %p, %zd, %d)\n", __func__, data, user_ptr, size, flags);
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

	scribe_uaccess_init();

	memset(kbuf, '\0', sizeof(kbuf));
	printf("kbuf = %p, ubuf = %p\n", kbuf, ubuf);
	ret = __copy_from_user(kbuf, ubuf, sizeof(ubuf));
	printf("ret = %lu, kbuf is %s\n", ret, kbuf);
	if (ret != 0) {
		errx(3, "Expected 0, got %lu\n", ret);
	}

	scribe_uaccess_verify(&scribe_uaccess_log_pre, true, kbuf, ubuf, sizeof(ubuf), SCRIBE_DATA_INPUT);
	scribe_uaccess_verify(&scribe_uaccess_log_post, true, kbuf, ubuf, sizeof(ubuf), SCRIBE_DATA_INPUT);

	scribe_uaccess_init();

	memset(kbuf, '\0', sizeof(kbuf));
	partial_ubuf = setup_partially_mapped_buffer(sizeof(UBUF_STRING));
	printf("kbuf = %p, partial_ubuf = %p\n", kbuf, partial_ubuf);
	ret = __copy_from_user(kbuf, partial_ubuf, sizeof(UBUF_STRING));
	printf("ret = %lu, kbuf is %s\n", ret, kbuf);
	if (ret != sizeof(UBUF_STRING)) {
		errx(3, "Expected %zu, got %ld", sizeof(UBUF_STRING), ret);
	}
	scribe_uaccess_verify(&scribe_uaccess_log_pre, true, kbuf, partial_ubuf, sizeof(UBUF_STRING), SCRIBE_DATA_INPUT);
	scribe_uaccess_verify(&scribe_uaccess_log_post, false, NULL, NULL, 0, 0);
}

static void
test___copy_to_user(void)
{
	char kbuf[] = UBUF_STRING;
	char ubuf[1024];
	char *partial_ubuf;
	unsigned long ret;

	scribe_uaccess_init();

	printf("kbuf = %p, ubuf = %p\n", kbuf, ubuf);
	ret = __copy_to_user(ubuf, kbuf, sizeof(kbuf));
	if (ret != 0)
		errx(3, "Expected %zu, got %ld", 0, ret);
	printf("ret = %lu, ubuf is %s\n", ret, ubuf);

	scribe_uaccess_verify(&scribe_uaccess_log_pre, true, kbuf, ubuf, sizeof(kbuf), 0);
	scribe_uaccess_verify(&scribe_uaccess_log_post, true, kbuf, ubuf, sizeof(kbuf), 0);

	scribe_uaccess_init();

	partial_ubuf = setup_partially_mapped_buffer(sizeof(kbuf));
	printf("kbuf = %p, partial_ubuf = %p\n", kbuf, partial_ubuf);
	ret = __copy_to_user(partial_ubuf, kbuf, sizeof(kbuf));
	printf("ret = %lu, partial_ubuf is %.*s\n", ret, sizeof(kbuf) / 2, partial_ubuf);
	if (ret != (sizeof(kbuf) / 2)) {
		errx(3, "Expected %zu, got %ld", (sizeof(kbuf) / 2), ret);
	}

	scribe_uaccess_verify(&scribe_uaccess_log_pre, true, kbuf, partial_ubuf, sizeof(kbuf), 0);
	scribe_uaccess_verify(&scribe_uaccess_log_post, false, NULL, NULL, 0, 0);
}

static void
test_csum_partial_copy_from_user(void)
{
#define CPCFU_UBUF_STRING "TTSSTTS"
	char ubuf[] = CPCFU_UBUF_STRING;
	char kbuf[1024];
	char *partial_ubuf;
	unsigned csum;
	int err;
	const unsigned INITIAL_CSUM = 0xfe47;
	const unsigned 	verify_csum = 0x53a7a6ef;	/* XXX: compare against C version */

	scribe_uaccess_init();

	memset(kbuf, '\0', sizeof(kbuf));
	printf("kbuf = %p, ubuf = %p\n", kbuf, ubuf);
	err = 0;
	csum = csum_partial_copy_from_user(ubuf, kbuf, sizeof(ubuf), INITIAL_CSUM, &err);
	printf("err = %d, csum = %#x, kbuf is %s\n", err, csum, kbuf);
	if (err != 0) {
		errx(3, "Expected 0, got %d\n", err);
	}

	if (verify_csum != csum) {
		errx(3, "Checksum mismatch, expected %#x, calculated %#x\n", verify_csum, csum);
	}
	scribe_uaccess_verify(&scribe_uaccess_log_pre, true, kbuf, ubuf, sizeof(ubuf), SCRIBE_DATA_INPUT);
	scribe_uaccess_verify(&scribe_uaccess_log_post, true, kbuf, ubuf, sizeof(ubuf), SCRIBE_DATA_INPUT);

	scribe_uaccess_init();

	memset(kbuf, '\0', sizeof(kbuf));
	partial_ubuf = setup_partially_mapped_buffer(sizeof(CPCFU_UBUF_STRING));
	printf("kbuf = %p, partial_ubuf = %p\n", kbuf, partial_ubuf);
	err = 0;
	csum = csum_partial_copy_from_user(partial_ubuf, kbuf, sizeof(CPCFU_UBUF_STRING), INITIAL_CSUM, &err);
	printf("err = %d, csum = %#x, kbuf is %s\n", err, csum, kbuf);
	if (err != -EFAULT) {
		errx(3, "Expected %d, got %d\n", -EFAULT, err);
	}
	scribe_uaccess_verify(&scribe_uaccess_log_pre, true, kbuf, partial_ubuf, sizeof(ubuf), SCRIBE_DATA_INPUT);
	scribe_uaccess_verify(&scribe_uaccess_log_post, false, NULL, NULL, 0, 0);
}

static void
verify_zero_buf(char *addr, size_t sz)
{
	char *c;

	for (c = addr; c < (addr + sz); ++c) {
		if (*c) {
			errx(3, "Nonzero byte (%d) in buffer %p at offset %d\n", *c, addr, c - addr);
		}
	}
}

static void
test_clear_user(void)
{
	char ubuf[67];
	char *partial_ubuf;
	int ret;

	scribe_uaccess_init();
	memset(ubuf, 'A', sizeof(ubuf));
	ret = __clear_user(ubuf, sizeof(ubuf));
	if (ret != 0)
		errx(3, "Expected 0, got %d\n", errno);
	verify_zero_buf(ubuf, sizeof(ubuf));
	scribe_uaccess_verify(&scribe_uaccess_log_pre, true, NULL, ubuf, sizeof(ubuf), SCRIBE_DATA_ZERO);
	scribe_uaccess_verify(&scribe_uaccess_log_post, true, NULL, ubuf, sizeof(ubuf), SCRIBE_DATA_ZERO);

	scribe_uaccess_init();
	partial_ubuf = setup_partially_mapped_buffer(sizeof(UBUF_STRING));
	ret = __clear_user(partial_ubuf, sizeof(UBUF_STRING));

	/* on arm, __clear_user always returns the buffer size on error, even if it faulted partway through */
	if (ret != sizeof(UBUF_STRING))
		errx(3, "Expected %d, got %#x\n", sizeof(UBUF_STRING) - sizeof(UBUF_STRING) / 2, ret);

	verify_zero_buf(partial_ubuf, sizeof(UBUF_STRING) / 2);
	scribe_uaccess_verify(&scribe_uaccess_log_pre, true, NULL, partial_ubuf, sizeof(UBUF_STRING), SCRIBE_DATA_ZERO);
	scribe_uaccess_verify(&scribe_uaccess_log_post, true, NULL, partial_ubuf,
			      sizeof(UBUF_STRING) - (sizeof(UBUF_STRING) / 2), SCRIBE_DATA_ZERO);
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
	TC(csum_partial_copy_from_user)
	TC(clear_user)
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
		testcases[i].func();
		exit(0);
	}
	errx(2, "no such testcase: %s\n", testcase);
	return 0;
}
