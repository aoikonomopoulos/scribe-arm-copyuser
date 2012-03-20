#include <stdio.h>

#define __user

char ubuf[] = "teh text";

extern unsigned long   __copy_from_user(void *to, const void __user *from, unsigned long n);

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

int
main(void)
{
	char kbuf[1024] = "";

	printf("kbuf = %p, ubuf = %p\n", kbuf, ubuf);
	__copy_from_user(kbuf, ubuf, sizeof(ubuf));
	printf("buf is %s\n", kbuf);
	return 0;
}
