; NOTE: Do not autogenerate
; RUN: llc -mtriple=microblazeel -mattr=+barrel-shift < %s | FileCheck %s
; REQUIRES: microblaze-registered-target

; Zero-extend i32 -> i64 (unsigned int -> unsigned long long).
;
; The Xilinx GCC tree adds an explicit zero_extendsidi2 pattern, but on a 32-bit
; target this widening is just "copy low word, clear high word".  The LLVM
; legalizer already emits that optimally, so no dedicated pattern is needed.
; This test proves the lowering is already minimal (regression guard).

; zext -> low word is the source reg, high word is zero.
; CHECK-LABEL: zext:
; CHECK-DAG:   addk r3, r5, r0
; CHECK-DAG:   addik r4, r0, 0
define i64 @zext(i32 %x) {
  %z = zext i32 %x to i64
  ret i64 %z
}

; zext feeding a 64-bit add: low add + carry-add, high addend is zero.
; No spill, no libcall.
; CHECK-LABEL: zext_add:
; CHECK:       add r3, r5, r7
; CHECK:       addik r4, r0, 0
; CHECK:       addc r4, r6, r4
; CHECK-NOT:   bral
define i64 @zext_add(i64 %acc, i32 %x) {
  %z = zext i32 %x to i64
  %r = add i64 %acc, %z
  ret i64 %r
}
