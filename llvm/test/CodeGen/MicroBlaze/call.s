	.file	"call.ll"
	.text
	.globl	leaf                            # -- Begin function leaf
	.p2align	2
	.type	leaf,@function
leaf:                                   # @leaf
	.cfi_startproc
# %bb.0:
	addk	r3, r5, r6
	rtsd	r15, 8
.Lfunc_end0:
	.size	leaf, .Lfunc_end0-leaf
	.cfi_endproc
                                        # -- End function
	.globl	caller                          # -- Begin function caller
	.p2align	2
	.type	caller,@function
caller:                                 # @caller
	.cfi_startproc
# %bb.0:
	addik	r1, r1, -4
	swi	r15, r1, 0
	bralid	r15, ext
	lwi	r15, r1, 0
	addik	r1, r1, 4
	rtsd	r15, 8
.Lfunc_end1:
	.size	caller, .Lfunc_end1-caller
	.cfi_endproc
                                        # -- End function
	.globl	six_args                        # -- Begin function six_args
	.p2align	2
	.type	six_args,@function
six_args:                               # @six_args
	.cfi_startproc
# %bb.0:
	addik	r1, r1, -8
	swi	r15, r1, 0
	swi	r19, r1, 4
	addk	r19, r7, r0
	bralid	r15, ext
	addk	r3, r3, r19
	lwi	r19, r1, 4
	lwi	r15, r1, 0
	addik	r1, r1, 8
	rtsd	r15, 8
.Lfunc_end2:
	.size	six_args, .Lfunc_end2-six_args
	.cfi_endproc
                                        # -- End function
	.globl	caller_many                     # -- Begin function caller_many
	.p2align	2
	.type	caller_many,@function
caller_many:                            # @caller_many
	.cfi_startproc
# %bb.0:
	addik	r1, r1, -8
	swi	r15, r1, 0
	swi	r19, r1, 4
	addk	r19, r7, r0
	bralid	r15, ext
	addk	r3, r3, r19
	lwi	r19, r1, 4
	lwi	r15, r1, 0
	addik	r1, r1, 8
	rtsd	r15, 8
.Lfunc_end3:
	.size	caller_many, .Lfunc_end3-caller_many
	.cfi_endproc
                                        # -- End function
	.section	".note.GNU-stack","",@progbits
