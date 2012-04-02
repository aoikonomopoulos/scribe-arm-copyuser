/*
 *  linux/arch/arm/lib/csumpartialcopyuser.S
 *
 *  Copyright (C) 1995-1998 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 27/03/03 Ian Molton Clean up CONFIG_CPU
 *
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/errno.h>

		.text

		.macro	save_regs
#ifdef CONFIG_SCRIBE
		@ save the values that might be clobbered by the call
		@ (we'll need them on the exit path too)
		stmfd	sp!, {r0 - r3, lr}
#define SCRIBE_SAVED_SIZE	(5 * 4)
		@ swap r0, r1, the order of kptr, uptr is reversed
		@ in scribe_pre_uaccess
		mov	r0, r1
		ldr	r1, [sp]
		@ r2 already holds the size
		mov	r3, #SCRIBE_DATA_INPUT
		bl	scribe_pre_uaccess
		@ restore without updating sp (we'll need the args on exit too)
		ldmfd	sp, {r0 - r3, lr}
		@ Note: we need to do all this before the following stmfd, since
		@ the code in csumpartialcopygeneric.S references the saved
		@ values
#endif	/* CONFIG_SCRIBE */
		stmfd	sp!, {r1, r2, r4 - r8, lr}	@ 8 regs * 4 bytes
#define SAVED_SIZE (8 * 4)
		.endm

		.macro	_load_regs_noscribe
		ldmfd	sp!, {r1, r2, r4 - r8, pc}
		.endm

		.macro	load_regs
#ifdef CONFIG_SCRIBE
		@ load the saved arguments (while exchanging r0, r1)
		ldr	r1, [sp, #SAVED_SIZE]
		str	r0, [sp, #SAVED_SIZE]	@ save return value
		ldr	r0, [sp, #(SAVED_SIZE + 4)]
		str	r3, [sp, #(SAVED_SIZE + 4)]	@ XXX: not needed
		ldr	r2, [sp, #(SAVED_SIZE + 8)]
		@ XXX: clobber r3
		mov	r3, #SCRIBE_DATA_INPUT
		bl	scribe_post_uaccess
		@
		@ ldmfd	sp!, {r1, r2, r4 - r8, pc}
		ldr	r0, [sp, #SAVED_SIZE]	@ restore return value
		ldr	r3, [sp, #(SAVED_SIZE + 4)]
		@ load everything but the pc
		ldmfd	sp!, {r1, r2, r4 - r8}
		@ [sp] contains the return address, but we need to adjust
		@ sp before returning
		add	sp, sp, #(4 + SCRIBE_SAVED_SIZE - 4)	@ drop previous saved lr, r0 - r3
		ldmfd	sp!, {pc}
#else	/* !CONFIG_SCRIBE */
		_load_regs_noscribe
#endif	/* CONFIG_SCRIBE */
		.endm

		.macro	load1b,	reg1
		ldrusr	\reg1, r0, 1
		.endm

		.macro	load2b, reg1, reg2
		ldrusr	\reg1, r0, 1
		ldrusr	\reg2, r0, 1
		.endm

		.macro	load1l, reg1
		ldrusr	\reg1, r0, 4
		.endm

		.macro	load2l, reg1, reg2
		ldrusr	\reg1, r0, 4
		ldrusr	\reg2, r0, 4
		.endm

		.macro	load4l, reg1, reg2, reg3, reg4
		ldrusr	\reg1, r0, 4
		ldrusr	\reg2, r0, 4
		ldrusr	\reg3, r0, 4
		ldrusr	\reg4, r0, 4
		.endm

/*
 * unsigned int
 * csum_partial_copy_from_user(const char *src, char *dst, int len, int sum, int *err_ptr)
 *  r0 = src, r1 = dst, r2 = len, r3 = sum, [sp] = *err_ptr
 *  Returns : r0 = checksum, [[sp, #0], #0] = 0 or -EFAULT
 */

#define FN_ENTRY	ENTRY(csum_partial_copy_from_user)
#define FN_EXIT		ENDPROC(csum_partial_copy_from_user)

#include "csumpartialcopygeneric.S"

/*
 * FIXME: minor buglet here
 * We don't return the checksum for the data present in the buffer.  To do
 * so properly, we would have to add in whatever registers were loaded before
 * the fault, which, with the current asm above is not predictable.
 */
		.pushsection .fixup,"ax"
		.align	4
9001:		mov	r4, #-EFAULT
#ifdef CONFIG_SCRIBE
		ldr	r5, [sp, #(SAVED_SIZE + SCRIBE_SAVED_SIZE)]
#else	/* !CONFIG_SCRIBE */
		ldr	r5, [sp, #8*4]		@ *err_ptr
#endif	/* CONFIG_SCRIBE */
		str	r4, [r5]
		ldmia	sp, {r1, r2}		@ retrieve dst, len
		add	r2, r2, r1
		mov	r0, #0			@ zero the buffer
9002:		teq	r2, r1
		strneb	r0, [r1], #1
		bne	9002b
		_load_regs_noscribe
		.popsection
