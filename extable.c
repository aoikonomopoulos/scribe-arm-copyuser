
#include <stdio.h>
#include <stdlib.h>

struct exception_table_entry {
	void *addr;
	void *fixup;
};

extern struct exception_table_entry __start___ex_table[];
extern struct exception_table_entry __stop___ex_table[];

void
print_exception_table(void)
{
	int i;

	printf("BEGIN exception table\n");
	for (i = 0; &__start___ex_table[i] != &__stop___ex_table[0]; ++i) {
		struct exception_table_entry *e;
		e = &__start___ex_table[i];
		printf("\t%p -> %p\n", e->addr, e->fixup);
	}
	printf("END exception table\n");
}

struct exception_table_entry *
search_exception_table(void *addr)
{
	int i;

	for (i = 0; &__start___ex_table[i] != &__stop___ex_table[0]; ++i) {
		struct exception_table_entry *e;
		e = &__start___ex_table[i];
		if (e->addr == addr)
			return e;
	}
	return NULL;
}
