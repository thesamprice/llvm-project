	.file	"fpu.ll"
	.text
	.globl	fadd_test                       # -- Begin function fadd_test
	.p2align	2
	.type	fadd_test,@function
fadd_test:                              # @fadd_test
	.cfi_startproc
# %bb.0:
	fadd	r3, r5, r6
	rtsd	r15, 8
	nop
.Lfunc_end0:
	.size	fadd_test, .Lfunc_end0-fadd_test
	.cfi_endproc
                                        # -- End function
	.globl	fsub_test                       # -- Begin function fsub_test
	.p2align	2
	.type	fsub_test,@function
fsub_test:                              # @fsub_test
	.cfi_startproc
# %bb.0:
	frsub	r3, r6, r5
	rtsd	r15, 8
	nop
.Lfunc_end1:
	.size	fsub_test, .Lfunc_end1-fsub_test
	.cfi_endproc
                                        # -- End function
	.globl	fmul_test                       # -- Begin function fmul_test
	.p2align	2
	.type	fmul_test,@function
fmul_test:                              # @fmul_test
	.cfi_startproc
# %bb.0:
	fmul	r3, r5, r6
	rtsd	r15, 8
	nop
.Lfunc_end2:
	.size	fmul_test, .Lfunc_end2-fmul_test
	.cfi_endproc
                                        # -- End function
	.globl	fdiv_test                       # -- Begin function fdiv_test
	.p2align	2
	.type	fdiv_test,@function
fdiv_test:                              # @fdiv_test
	.cfi_startproc
# %bb.0:
	fdiv	r3, r5, r6
	rtsd	r15, 8
	nop
.Lfunc_end3:
	.size	fdiv_test, .Lfunc_end3-fdiv_test
	.cfi_endproc
                                        # -- End function
	.globl	fsqrt_test                      # -- Begin function fsqrt_test
	.p2align	2
	.type	fsqrt_test,@function
fsqrt_test:                             # @fsqrt_test
	.cfi_startproc
# %bb.0:
	fsqrt	r3, r5
	rtsd	r15, 8
	nop
.Lfunc_end4:
	.size	fsqrt_test, .Lfunc_end4-fsqrt_test
	.cfi_endproc
                                        # -- End function
	.globl	sint_to_fp_test                 # -- Begin function sint_to_fp_test
	.p2align	2
	.type	sint_to_fp_test,@function
sint_to_fp_test:                        # @sint_to_fp_test
	.cfi_startproc
# %bb.0:
	flt	r3, r5
	rtsd	r15, 8
	nop
.Lfunc_end5:
	.size	sint_to_fp_test, .Lfunc_end5-sint_to_fp_test
	.cfi_endproc
                                        # -- End function
	.globl	fp_to_sint_test                 # -- Begin function fp_to_sint_test
	.p2align	2
	.type	fp_to_sint_test,@function
fp_to_sint_test:                        # @fp_to_sint_test
	.cfi_startproc
# %bb.0:
	fint	r3, r5
	rtsd	r15, 8
	nop
.Lfunc_end6:
	.size	fp_to_sint_test, .Lfunc_end6-fp_to_sint_test
	.cfi_endproc
                                        # -- End function
	.globl	fcmp_select_lt                  # -- Begin function fcmp_select_lt
	.p2align	2
	.type	fcmp_select_lt,@function
fcmp_select_lt:                         # @fcmp_select_lt
	.cfi_startproc
# %bb.0:
	fcmp.gt	r4, r5, r6
	bneid	r4, .LBB7_2
	addk	r3, r7, r0
.LBB7_1:
	addk	r3, r8, r0
.LBB7_2:
	rtsd	r15, 8
	nop
.Lfunc_end7:
	.size	fcmp_select_lt, .Lfunc_end7-fcmp_select_lt
	.cfi_endproc
                                        # -- End function
	.globl	fcmp_select_ult                 # -- Begin function fcmp_select_ult
	.p2align	2
	.type	fcmp_select_ult,@function
fcmp_select_ult:                        # @fcmp_select_ult
	.cfi_startproc
# %bb.0:
	fcmp.gt	r4, r5, r6
	fcmp.un	r5, r5, r6
	or	r4, r4, r5
	bneid	r4, .LBB8_2
	addk	r3, r7, r0
.LBB8_1:
	addk	r3, r8, r0
.LBB8_2:
	rtsd	r15, 8
	nop
.Lfunc_end8:
	.size	fcmp_select_ult, .Lfunc_end8-fcmp_select_ult
	.cfi_endproc
                                        # -- End function
	.globl	fcmp_select_ord                 # -- Begin function fcmp_select_ord
	.p2align	2
	.type	fcmp_select_ord,@function
fcmp_select_ord:                        # @fcmp_select_ord
	.cfi_startproc
# %bb.0:
	fcmp.un	r4, r5, r6
	beqid	r4, .LBB9_2
	addk	r3, r7, r0
.LBB9_1:
	addk	r3, r8, r0
.LBB9_2:
	rtsd	r15, 8
	nop
.Lfunc_end9:
	.size	fcmp_select_ord, .Lfunc_end9-fcmp_select_ord
	.cfi_endproc
                                        # -- End function
	.globl	fcmp_fselect_lt                 # -- Begin function fcmp_fselect_lt
	.p2align	2
	.type	fcmp_fselect_lt,@function
fcmp_fselect_lt:                        # @fcmp_fselect_lt
	.cfi_startproc
# %bb.0:
	fcmp.gt	r4, r5, r6
	bneid	r4, .LBB10_2
	addk	r3, r7, r0
.LBB10_1:
	addk	r3, r8, r0
.LBB10_2:
	rtsd	r15, 8
	nop
.Lfunc_end10:
	.size	fcmp_fselect_lt, .Lfunc_end10-fcmp_fselect_lt
	.cfi_endproc
                                        # -- End function
	.section	".note.GNU-stack","",@progbits
