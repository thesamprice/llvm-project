	.file	"frame.ll"
	.text
	.globl	with_alloca                     # -- Begin function with_alloca
	.p2align	2
	.type	with_alloca,@function
with_alloca:                            # @with_alloca
	.cfi_startproc
# %bb.0:
	addik	r1, r1, -4
	addk	r3, r5, r0
	swi	r3, r1, 0
	addik	r1, r1, 4
	rtsd	r15, 8
.Lfunc_end0:
	.size	with_alloca, .Lfunc_end0-with_alloca
	.cfi_endproc
                                        # -- End function
	.globl	with_call_and_local             # -- Begin function with_call_and_local
	.p2align	2
	.type	with_call_and_local,@function
with_call_and_local:                    # @with_call_and_local
	.cfi_startproc
# %bb.0:
	addik	r1, r1, -8
	swi	r15, r1, 0
	swi	r5, r1, 4
	addik	r5, r1, 4
	bralid	r15, use
	lwi	r15, r1, 0
	addik	r1, r1, 8
	rtsd	r15, 8
.Lfunc_end1:
	.size	with_call_and_local, .Lfunc_end1-with_call_and_local
	.cfi_endproc
                                        # -- End function
	.globl	read_global                     # -- Begin function read_global
	.p2align	2
	.type	read_global,@function
read_global:                            # @read_global
	.cfi_startproc
# %bb.0:
	addik	r3, r0, g
	lwi	r3, r3, 0
	rtsd	r15, 8
.Lfunc_end2:
	.size	read_global, .Lfunc_end2-read_global
	.cfi_endproc
                                        # -- End function
	.type	g,@object                       # @g
	.section	.bss,"aw",@nobits
	.globl	g
	.p2align	2, 0x0
g:
	.long	0                               # 0x0
	.size	g, 4

	.section	".note.GNU-stack","",@progbits
