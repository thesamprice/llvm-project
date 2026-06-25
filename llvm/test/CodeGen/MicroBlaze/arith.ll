; RUN: llc -mtriple=microblazeel < %s | FileCheck %s

; CHECK-LABEL: add_i32:
; CHECK: addk r3, r5, r6
; CHECK: rtsd r15, 8
define i32 @add_i32(i32 %a, i32 %b) {
  %r = add i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: sub_i32:
; CHECK: rsubk r3, r6, r5
; CHECK: rtsd r15, 8
define i32 @sub_i32(i32 %a, i32 %b) {
  %r = sub i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: mul_i32:
; CHECK: mul r3, r5, r6
; CHECK: rtsd r15, 8
define i32 @mul_i32(i32 %a, i32 %b) {
  %r = mul i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: and_i32:
; CHECK: and r3, r5, r6
; CHECK: rtsd r15, 8
define i32 @and_i32(i32 %a, i32 %b) {
  %r = and i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: or_i32:
; CHECK: or r3, r5, r6
; CHECK: rtsd r15, 8
define i32 @or_i32(i32 %a, i32 %b) {
  %r = or i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: xor_i32:
; CHECK: xor r3, r5, r6
; CHECK: rtsd r15, 8
define i32 @xor_i32(i32 %a, i32 %b) {
  %r = xor i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: add_imm:
; CHECK: addik r3, r5, 42
; CHECK: rtsd r15, 8
define i32 @add_imm(i32 %a) {
  %r = add i32 %a, 42
  ret i32 %r
}

; CHECK-LABEL: three_args:
; CHECK: addk r3, r5, r6
; CHECK: addk r3, r3, r7
; CHECK: rtsd r15, 8
define i32 @three_args(i32 %a, i32 %b, i32 %c) {
  %t = add i32 %a, %b
  %r = add i32 %t, %c
  ret i32 %r
}
