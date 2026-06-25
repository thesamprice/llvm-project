; RUN: llc -mtriple=microblazeel < %s | FileCheck %s
; REQUIRES: microblaze-registered-target

; Sign-extension uses SEXT8/SEXT16 instructions (UG984 §5).
; Zero-extension via AND with immediate mask.

; CHECK-LABEL: sext8:
; CHECK: sext8 r3, r5
define i32 @sext8(i8 %a) {
  %r = sext i8 %a to i32
  ret i32 %r
}

; CHECK-LABEL: sext16:
; CHECK: sext16 r3, r5
define i32 @sext16(i16 %a) {
  %r = sext i16 %a to i32
  ret i32 %r
}

; Zero-extend i8: AND with 0xFF = andi r3, r5, 255
; CHECK-LABEL: zext8:
; CHECK: andi r3, r5, 255
define i32 @zext8(i8 %a) {
  %r = zext i8 %a to i32
  ret i32 %r
}

; Zero-extend i16: 65535 > signed-16-bit range → addik+and
; CHECK-LABEL: zext16:
; CHECK: addik r3, r0, 65535
; CHECK: and r3, r5, r3
define i32 @zext16(i16 %a) {
  %r = zext i16 %a to i32
  ret i32 %r
}
