#ifndef _EXTABLE_H_
#define _EXTABLE_H_

struct exception_table_entry {
	void *addr;
	void *fixup;
};

extern void print_exception_table(void);
extern struct exception_table_entry * search_exception_table(void *);

#endif /* _EXTABLE_H_ */
