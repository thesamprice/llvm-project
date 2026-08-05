; NOTE: Do not autogenerate
; delay-slot-load-use.ll — load-use hazard handling in the DSF.
;
; MicroBlaze has a 2-cycle load-to-use latency (IIC_LD=2).  A load at
; pipeline stage N makes its result available at N+2.  In the delay-slot
; context the branch occupies N+1 and the delay slot is N+2, so a load
; hoisted into the delay slot is always safe.
;
; Tests:
;   (a) A load whose result is the return value is hoisted into the
;       return (rtsd) delay slot.
;   (b) A load whose result feeds the branch condition register cannot be
;       hoisted: the load defines the same register that the branch reads
;       (caught by BranchGuardUses).  The forward branch is demoted to its
;       no-delay form (beqi, not beqid); no NOP is emitted.
;
; RUN: llc -mtriple=microblazeel-unknown-elf < %s | FileCheck %s
; REQUIRES: microblaze-registered-target
; (a) -----------------------------------------------------------------------
; Load is the only backward-scan candidate; it fills the rtsd delay slot.

define i32 @load_into_return_slot(ptr %p) {
; CHECK-LABEL: load_into_return_slot:
; CHECK:       rtsd r15, 8
; CHECK:       lwi {{r[0-9]+}},
    %v = load i32, ptr %p
    ret i32 %v
}

; (b) -----------------------------------------------------------------------
; beqid tests the loaded register; BranchGuardUses blocks hoisting the load.
; Forward branch → demoted to beqi (no delay slot, no NOP).

define i32 @load_feeds_branch_condition(ptr %p) {
; CHECK-LABEL: load_feeds_branch_condition:
; CHECK:       lwi [[REG:r[0-9]+]],
; CHECK:       beqi [[REG]],
; CHECK-NOT:   beqid
    %v = load i32, ptr %p
    %cmp = icmp eq i32 %v, 0
    br i1 %cmp, label %zero, label %nonzero
zero:
    ret i32 0
nonzero:
    ret i32 1
}
