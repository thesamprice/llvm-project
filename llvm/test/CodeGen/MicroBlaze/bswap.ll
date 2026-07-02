; RUN: llc -mtriple=microblazeel %s -o - | FileCheck %s
; REQUIRES: microblaze-registered-target

; Test that llvm.bswap.i32 is lowered correctly with and without +reorder.
;
; With +reorder (via per-function "target-features" attribute):
;   bswap32(x) = swaph(swapb(x))   — 2 instructions
; Without +reorder: software expansion via shift library calls.

; CHECK-LABEL: bswap_with_reorder:
; CHECK:       swapb r3, r5
; CHECK-NEXT:  swaph r3, r3
; CHECK-NOT:   bralid

define i32 @bswap_with_reorder(i32 %x) "target-features"="+reorder" {
  %r = call i32 @llvm.bswap.i32(i32 %x)
  ret i32 %r
}

; CHECK-LABEL: bswap_no_reorder:
; CHECK:       bralid {{r[0-9]+}}, __lshrsi3

define i32 @bswap_no_reorder(i32 %x) {
  %r = call i32 @llvm.bswap.i32(i32 %x)
  ret i32 %r
}

declare i32 @llvm.bswap.i32(i32)
