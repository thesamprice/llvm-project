; RUN: llc -mtriple=microblazeel -mattr=+barrel-shift < %s | FileCheck %s
; REQUIRES: microblaze-registered-target

; Register-form shifts (variable shift amount) → BSRL/BSRA/BSLL

; CHECK-LABEL: srl_reg:
; CHECK: bsrl r3, r5, r6
; CHECK: rtsd r15, 8
define i32 @srl_reg(i32 %a, i32 %b) {
  %r = lshr i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: sra_reg:
; CHECK: bsra r3, r5, r6
; CHECK: rtsd r15, 8
define i32 @sra_reg(i32 %a, i32 %b) {
  %r = ashr i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: shl_reg:
; CHECK: bsll r3, r5, r6
; CHECK: rtsd r15, 8
define i32 @shl_reg(i32 %a, i32 %b) {
  %r = shl i32 %a, %b
  ret i32 %r
}

; Immediate-form shifts (constant shift amount) → BSRLI/BSRAI/BSLLI

; CHECK-LABEL: srl_imm:
; CHECK: bsrli r3, r5, 5
; CHECK: rtsd r15, 8
define i32 @srl_imm(i32 %a) {
  %r = lshr i32 %a, 5
  ret i32 %r
}

; ashr by 1 uses the dedicated single-bit 'sra' instruction rather than bsrai.
; CHECK-LABEL: sra_imm:
; CHECK: sra r3, r5
; CHECK: rtsd r15, 8
define i32 @sra_imm(i32 %a) {
  %r = ashr i32 %a, 1
  ret i32 %r
}

; CHECK-LABEL: shl_imm:
; CHECK: bslli r3, r5, 3
; CHECK: rtsd r15, 8
define i32 @shl_imm(i32 %a) {
  %r = shl i32 %a, 3
  ret i32 %r
}

; Shift-by-31 (maximum uimm5 value)
; CHECK-LABEL: srl_31:
; CHECK: bsrli r3, r5, 31
define i32 @srl_31(i32 %a) {
  %r = lshr i32 %a, 31
  ret i32 %r
}

; Shift-by-1 (minimum nonzero)
; CHECK-LABEL: shl_1:
; CHECK: bslli r3, r5, 1
define i32 @shl_1(i32 %a) {
  %r = shl i32 %a, 1
  ret i32 %r
}
