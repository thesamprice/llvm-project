	.file	"branch.ll"
	.text
	.globl	max                             # -- Begin function max
	.p2align	2
	.type	max,@function
max:                                    # @max
	.cfi_startproc
# %bb.0:
	rsubk	r3, r6, r5
	bleid	r3, .LBB0_2
	bri	.LBB0_1
.LBB0_1:                                # %then
	addk	r3, r5, r0
	rtsd	r15, 8
.LBB0_2:                                # %else
	addk	r3, r6, r0
	rtsd	r15, 8
.Lfunc_end0:
	.size	max, .Lfunc_end0-max
	.cfi_endproc
                                        # -- End function
	.globl	min                             # -- Begin function min
	.p2align	2
	.type	min,@function
min:                                    # @min
	.cfi_startproc
# %bb.0:
	rsubk	r3, r6, r5
	bgeid	r3, .LBB1_2
	bri	.LBB1_1
.LBB1_1:                                # %then
	addk	r3, r5, r0
	rtsd	r15, 8
.LBB1_2:                                # %else
	addk	r3, r6, r0
	rtsd	r15, 8
.Lfunc_end1:
	.size	min, .Lfunc_end1-min
	.cfi_endproc
                                        # -- End function
	.globl	loop_phi                        # -- Begin function loop_phi
	.p2align	2
	.type	loop_phi,@function
loop_phi:                               # @loop_phi
	.cfi_startproc
# %bb.0:                                # %entry
	addik	r3, r0, 0
.LBB2_1:                                # %loop
                                        # =>This Inner Loop Header: Depth=1
	addik	r3, r3, 1
	rsubk	r4, r5, r3
	bltid	r4, .LBB2_1
	bri	.LBB2_2
.LBB2_2:                                # %done
	rtsd	r15, 8
.Lfunc_end2:
	.size	loop_phi, .Lfunc_end2-loop_phi
	.cfi_endproc
                                        # -- End function
	.section	".note.GNU-stack","",@progbits
