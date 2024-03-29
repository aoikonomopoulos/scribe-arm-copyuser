/*
 *  linux/arch/arm/lib/copy_to_user.S
 *
 *  Author:	Nicolas Pitre
 *  Created:	Sep 29, 2005
 *  Copyright:	MontaVista Software, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/linkage.h>
#include <asm/assembler.h>

/*
 * Prototype:
 *
 *	size_t __copy_to_user(void *to, const void *from, size_t n)
 *
 * Purpose:
 *
 *	copy a block to user memory from kernel memory
 *
 * Params:
 *
 *	to = user memory
 *	from = kernel memory
 *	n = number of bytes to copy
 *
 * Return value:
 *
 *	Number of bytes NOT copied.
 */

#define LDR1W_SHIFT	0
#ifndef CONFIG_THUMB2_KERNEL
#define STR1W_SHIFT	0
#else
#define STR1W_SHIFT	1
#endif

	.macro ldr1w ptr reg abort
	W(ldr) \reg, [\ptr], #4
	.endm

	.macro ldr4w ptr reg1 reg2 reg3 reg4 abort
	ldmia \ptr!, {\reg1, \reg2, \reg3, \reg4}
	.endm

	.macro ldr8w ptr reg1 reg2 reg3 reg4 reg5 reg6 reg7 reg8 abort
	ldmia \ptr!, {\reg1, \reg2, \reg3, \reg4, \reg5, \reg6, \reg7, \reg8}
	.endm

	.macro ldr1b ptr reg cond=al abort
	ldr\cond\()b \reg, [\ptr], #1
	.endm

	.macro str1w ptr reg abort
	strusr	\reg, \ptr, 4, abort=\abort
	.endm

	.macro str8w ptr reg1 reg2 reg3 reg4 reg5 reg6 reg7 reg8 abort
	str1w \ptr, \reg1, \abort
	str1w \ptr, \reg2, \abort
	str1w \ptr, \reg3, \abort
	str1w \ptr, \reg4, \abort
	str1w \ptr, \reg5, \abort
	str1w \ptr, \reg6, \abort
	str1w \ptr, \reg7, \abort
	str1w \ptr, \reg8, \abort
	.endm

	.macro str1b ptr reg cond=al abort
	strusr	\reg, \ptr, 1, \cond, abort=\abort
	.endm

	.macro enter reg1 reg2
	mov	r3, #0
	stmdb	sp!, {r0, r2, r3, \reg1, \reg2}
#ifdef CONFIG_SCRIBE
	@ r0 == uptr, r1 == kptr, those just need to be exchanged, but r1 needs to be saved
	@ however, we save (again) r2 and r3, because their stack offsets depend
	@ on whether reg1 or reg2 happens to be r1. r0 is always at the same offset.
	stmfd sp!, {r1, r2, r3, lr}
	@ exchange r0, r1
	mov r3, r0
	mov r0, r1
	mov r1, r3
	@ r2 (size) is already setup
	@ r3 (flags) needs to be loaded up w/ a constant
	mov r3, #0
	bl scribe_pre_uaccess
	ldmfd sp, {r1, r2, r3, lr}	@ note: does not update sp, b/c we need those values at exit as well
	ldr r0, [sp, #16]
#endif	/* CONFIG_SCRIBE */
	.endm

	.macro exit reg1 reg2
#ifdef CONFIG_SCRIBE
	@ save regs that might get clobbered by the call
	stmfd sp!, {r1, r2, r3, lr}
	@ load up the argument values we saved on entry, but
	@ exchange r0, r1
	ldr r1, [sp, #32]
	ldr r2, [sp, #20]
	ldr r0, [sp, #16]
	mov r3, #0
	bl scribe_post_uaccess
	@ restore regs (not r0, it gets immediately clobbered below)
	ldmfd sp!, {r1, r2, r3, lr}
	@ get rid of the registers we saved in enter
	add sp, sp, #16
#endif	/* CONFIG_SCRIBE */
	add	sp, sp, #8
	ldmfd	sp!, {r0, \reg1, \reg2}
	.endm

	.text

ENTRY(__copy_to_user_std)
WEAK(__copy_to_user)

#include "copy_template.S"

ENDPROC(__copy_to_user)
ENDPROC(__copy_to_user_std)

	.pushsection .fixup,"ax"
	.align 0
	copy_abort_preamble
#ifdef CONFIG_SCRIBE
	add sp, sp, #16
#endif	/* CONFIG_SCRIBE */
	ldmfd	sp!, {r1, r2, r3}
	sub	r0, r0, r1
	rsb	r0, r0, r2
	copy_abort_end
	.popsection

