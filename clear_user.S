/*
 *  linux/arch/arm/lib/clear_user.S
 *
 *  Copyright (C) 1995, 1996,1997,1998 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>

		.text

/* Prototype: int __clear_user(void *addr, size_t sz)
 * Purpose  : clear some user memory
 * Params   : addr - user memory address to clear
 *          : sz   - number of bytes to clear
 * Returns  : number of bytes NOT cleared
 */
ENTRY(__clear_user_std)
WEAK(__clear_user)
#ifdef CONFIG_SCRIBE
		stmfd	sp!, {r0-r12, lr}
		mov	r2, r1		@ len
		mov	r1, r0		@ uptr
		mov	r0, #0		@ kptr == NULL
		mov	r3, #SCRIBE_DATA_ZERO
		bl	scribe_pre_uaccess
		@ do not update sp, we need the args on exit too
		ldmfd	sp, {r0-r12}
#endif	/* CONFIG_SCRIBE */
		stmfd	sp!, {r1, lr}
		mov	r2, #0
		cmp	r1, #4
		blt	2f
		ands	ip, r0, #3
		beq	1f
		cmp	ip, #2
		strusr	r2, r0, 1
		strusr	r2, r0, 1, le
		strusr	r2, r0, 1, lt
		rsb	ip, ip, #4
		sub	r1, r1, ip		@  7  6  5  4  3  2  1
1:		subs	r1, r1, #8		@ -1 -2 -3 -4 -5 -6 -7
		strusr	r2, r0, 4, pl, rept=2
		bpl	1b
		adds	r1, r1, #4		@  3  2  1  0 -1 -2 -3
		strusr	r2, r0, 4, pl
2:		tst	r1, #2			@ 1x 1x 0x 0x 1x 1x 0x
		strusr	r2, r0, 1, ne, rept=2
		tst	r1, #1			@ x1 x0 x1 x0 x1 x0 x1
		it	ne			@ explicit IT needed for the label
USER(		strnebt	r2, [r0])
		mov	r0, #0
#ifdef CONFIG_SCRIBE
		ldr	r1, [sp, #8]	@ uptr
		@ the only valuable value here is in r0, save it over our
		@ stack copy of the initial r0 value
		str	r0, [sp, #8]
		@ r0 is the bytes remaining, we need to call scribe_post_uaccess
		@ with bytes copied. Use the copy of the initial r1 value
		@ on the stack
		ldr	r2, [sp, #12]
		mov	r0, #0		@ NULL
		mov	r3, #SCRIBE_DATA_ZERO
		bl	scribe_post_uaccess
		add	sp, sp, #8	@ drop saved {r1, lr}
		ldmfd	sp!, {r0-r12, pc}
#else	/* !CONFIG_SCRIBE */
		ldmfd	sp!, {r1, pc}
#endif	/* CONFIG_SCRIBE */

ENDPROC(__clear_user)
ENDPROC(__clear_user_std)

		.pushsection .fixup,"ax"
		.align	0
9001:
#ifdef CONFIG_SCRIBE
		@ Need to calculate the number of bytes copied.
		@ r0 holds the faulting user address and we have
		@ saved the base address of the user buffer on
		@ entry.
		ldr	r1, [sp, #8]	@ uptr
		sub	r2, r0, r1	@ calculate scribe len
		mov	r0, #0		@ NULL
		mov	r3, #SCRIBE_DATA_ZERO
		bl	scribe_post_uaccess
		@ The !CONFIG_SCRIBE asm always returns length on error
		ldr	r0, [sp, #0]
		str	r0, [sp, #8]
		add	sp, sp, #8	@ drop saved {r1, lr}
		ldmfd	sp!, {r0-r12, pc}
#else	/* !CONFIG_SCRIBE */
		ldmfd	sp!, {r0, pc}
#endif	/* CONFIG_SCRIBE */
		.popsection

