; RUN: llc -mtriple=microblazeel -verify-machineinstrs < %s | FileCheck %s

; Check that __builtin_frame_address(0) and __builtin_return_address(0) are
; lowered to register copies rather than crashing in the MC layer.
;
; FRAMEADDR(0) → R1 (the stack pointer; MicroBlaze has no separate frame
; pointer, so the SP at function entry is the closest approximation).
;
; RETURNADDR(0) → R15 (the hardware link register, read at function entry
; before any callee-saved-register spill or call could overwrite it).
;
; Depth > 0 for either intrinsic requires stack-frame chaining that MicroBlaze
; does not support and is reported as a fatal error.

; CHECK-LABEL: frame_addr_depth0:
define ptr @frame_addr_depth0() nounwind {
  %fa = call ptr @llvm.frameaddress.p0(i32 0)
  ret ptr %fa
}
; The frame address is R1; returning in R3 expands to addk r3, r1, r0.
; The delay-slot filler hoists addk into the rtsd delay slot.
; CHECK: rtsd r15, 8
; CHECK-NEXT: addk r3, r1, r0

; CHECK-LABEL: return_addr_depth0:
define ptr @return_addr_depth0() nounwind {
  %ra = call ptr @llvm.returnaddress(i32 0)
  ret ptr %ra
}
; The return address is R15; returning in R3 expands to addk r3, r15, r0.
; The delay-slot filler hoists addk into the rtsd delay slot.
; CHECK: rtsd r15, 8
; CHECK-NEXT: addk r3, r15, r0

declare ptr @llvm.frameaddress.p0(i32 immarg)
declare ptr @llvm.returnaddress(i32 immarg)
