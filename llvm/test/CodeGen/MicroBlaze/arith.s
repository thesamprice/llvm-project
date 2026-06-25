	.file	"arith.ll"
	.text
	.globl	add_i32                         # -- Begin function add_i32
	.p2align	2
	.type	add_i32,@function
add_i32:                                # @add_i32
	.cfi_startproc
# %bb.0:
	addk	r3, r5, r6
	rtsd	r15, 8
.Lfunc_end0:
	.size	add_i32, .Lfunc_end0-add_i32
	.cfi_endproc
                                        # -- End function
	.globl	sub_i32                         # -- Begin function sub_i32
	.p2align	2
	.type	sub_i32,@function
sub_i32:                                # @sub_i32
	.cfi_startproc
# %bb.0:
	rsubk	r3, r6, r5
	rtsd	r15, 8
.Lfunc_end1:
	.size	sub_i32, .Lfunc_end1-sub_i32
	.cfi_endproc
                                        # -- End function
	.globl	mul_i32                         # -- Begin function mul_i32
	.p2align	2
	.type	mul_i32,@function
mul_i32:                                # @mul_i32
	.cfi_startproc
# %bb.0:
	mul	r3, r5, r6
	rtsd	r15, 8
.Lfunc_end2:
	.size	mul_i32, .Lfunc_end2-mul_i32
	.cfi_endproc
                                        # -- End function
	.globl	and_i32                         # -- Begin function and_i32
	.p2align	2
	.type	and_i32,@function
and_i32:                                # @and_i32
	.cfi_startproc
# %bb.0:
	and	r3, r5, r6
	rtsd	r15, 8
.Lfunc_end3:
	.size	and_i32, .Lfunc_end3-and_i32
	.cfi_endproc
                                        # -- End function
	.globl	or_i32                          # -- Begin function or_i32
	.p2align	2
	.type	or_i32,@function
or_i32:                                 # @or_i32
	.cfi_startproc
# %bb.0:
	or	r3, r5, r6
	rtsd	r15, 8
.Lfunc_end4:
	.size	or_i32, .Lfunc_end4-or_i32
	.cfi_endproc
                                        # -- End function
	.globl	xor_i32                         # -- Begin function xor_i32
	.p2align	2
	.type	xor_i32,@function
xor_i32:                                # @xor_i32
	.cfi_startproc
# %bb.0:
	xor	r3, r5, r6
	rtsd	r15, 8
.Lfunc_end5:
	.size	xor_i32, .Lfunc_end5-xor_i32
	.cfi_endproc
                                        # -- End function
	.globl	add_imm                         # -- Begin function add_imm
	.p2align	2
	.type	add_imm,@function
add_imm:                                # @add_imm
	.cfi_startproc
# %bb.0:
	addik	r3, r5, 42
	rtsd	r15, 8
.Lfunc_end6:
	.size	add_imm, .Lfunc_end6-add_imm
	.cfi_endproc
                                        # -- End function
	.globl	three_args                      # -- Begin function three_args
	.p2align	2
	.type	three_args,@function
three_args:                             # @three_args
	.cfi_startproc
# %bb.0:
	addk	r3, r5, r6
	addk	r3, r3, r7
	rtsd	r15, 8
.Lfunc_end7:
	.size	three_args, .Lfunc_end7-three_args
	.cfi_endproc
                                        # -- End function
	.section	".note.GNU-stack","",@progbits
