; NOTE: Do not autogenerate
; RUN: llc -mtriple=microblazeel -verify-machineinstrs %s -o - | FileCheck %s
; REQUIRES: microblaze-registered-target

; Test that llvm.abs.i32 lowers to a branch diamond (bgti[d] + rsubk) rather
; than the default arithmetic sequence (bsrai + xor + rsubk).

; The diamond:
;   headMBB: bgti[d] src, sinkMBB  (skip negation if src > 0)
;   negMBB:  rsubk dst, src, r0    (0 - src = -src; runs only when src <= 0)
;   sinkMBB: PHI dst = [dst_neg, negMBB], [src, headMBB]
;
; The branch is emitted as the delayed form (bgtid); the delay-slot filler keeps
; it delayed when it can move a safe instruction from sinkMBB into the slot and
; otherwise downgrades to the non-delayed bgti.  Either form is acceptable here.

; CHECK-LABEL: abs_i32:
; CHECK:       bgt{{i?d?}} {{r[0-9]+}}, .LBB
; CHECK-NOT:   bsrai
; CHECK-NOT:   xor
; CHECK:       rsubk {{r[0-9]+}}, {{r[0-9]+}}, r0

define i32 @abs_i32(i32 %x) {
  %r = call i32 @llvm.abs.i32(i32 %x, i1 false)
  ret i32 %r
}

; CHECK-LABEL: abs_poisonundef:
; CHECK:       bgt{{i?d?}} {{r[0-9]+}}, .LBB
; CHECK-NOT:   bsrai
; CHECK-NOT:   xor
; CHECK:       rsubk {{r[0-9]+}}, {{r[0-9]+}}, r0

define i32 @abs_poisonundef(i32 %x) {
  %r = call i32 @llvm.abs.i32(i32 %x, i1 true)
  ret i32 %r
}

declare i32 @llvm.abs.i32(i32, i1)
