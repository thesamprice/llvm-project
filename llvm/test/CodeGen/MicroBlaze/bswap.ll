; NOTE: Do not autogenerate
; RUN: llc -mtriple=microblazeel %s -o - | FileCheck %s
; REQUIRES: microblaze-registered-target

; Test that llvm.bswap.i32 is lowered correctly with and without +swapb/+reorder.
;
; SWAPB is a full 32-bit byte reversal (ABCD→DCBA), so bswap32 needs only 1
; instruction when +swapb (or +reorder which implies it) is available.
; Without either flag: Expand → shift/or sequence (no swapb emitted).

; CHECK-LABEL: bswap_with_reorder:
; CHECK:       swapb r3, r5
; CHECK-NOT:   swaph
; CHECK-NOT:   bralid

define i32 @bswap_with_reorder(i32 %x) "target-features"="+reorder" {
  %r = call i32 @llvm.bswap.i32(i32 %x)
  ret i32 %r
}

; CHECK-LABEL: bswap_swapb_only:
; CHECK:       swapb r3, r5
; CHECK-NOT:   swaph
; CHECK-NOT:   bralid

define i32 @bswap_swapb_only(i32 %x) "target-features"="+swapb" {
  %r = call i32 @llvm.bswap.i32(i32 %x)
  ret i32 %r
}

; CHECK-LABEL: bswap_no_reorder:
; CHECK-NOT:   swapb
; CHECK-NOT:   swaph

define i32 @bswap_no_reorder(i32 %x) {
  %r = call i32 @llvm.bswap.i32(i32 %x)
  ret i32 %r
}

declare i32 @llvm.bswap.i32(i32)
