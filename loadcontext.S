#include <linux/linkage.h>

ENTRY(loadcontext)
	@ r0 : struct sigcontext *
	add r0, #16		@ skip past the saved r0 value
	ldmfd r0, {r1-r14}
	ldr r2, [r0, #60]	@ load the cpsr value
	msr cpsr, r2		@ set cpsr
	ldr r2, [r0, #56]	@ pc
	str r2, [r0, #0]	@ overwrite r1's value (already restored) with the pc
	ldr r2, [r0, #4]	@ restore r2
	sub r0, #4
	@ restore r0 and the pc
	ldmfd r0, {r0,pc}
