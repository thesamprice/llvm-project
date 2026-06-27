; RUN: llc -mtriple=microblazeel -mattr=+hard-float,+float-convert -O2 < %s \
; RUN:   | FileCheck %s --check-prefix=HW
; RUN: llc -mtriple=microblazeel -O2 < %s \
; RUN:   | FileCheck %s --check-prefix=SOFT

; Verify that +hard-float emits hardware FPU instructions (UG984 §5 opcode 0x16)
; and that the default (soft-float) path emits compiler-rt libcalls.

; HW-LABEL: fadd_test:
; HW: fadd
; SOFT-LABEL: fadd_test:
; SOFT: bralid {{.*}}, __addsf3
define float @fadd_test(float %a, float %b) {
  %r = fadd float %a, %b
  ret float %r
}

; HW-LABEL: fsub_test:
; frsub rD, rA, rB = rB - rA; operands swapped to compute a - b.
; HW: frsub
; SOFT-LABEL: fsub_test:
; SOFT: bralid {{.*}}, __subsf3
define float @fsub_test(float %a, float %b) {
  %r = fsub float %a, %b
  ret float %r
}

; HW-LABEL: fmul_test:
; HW: fmul
; SOFT-LABEL: fmul_test:
; SOFT: bralid {{.*}}, __mulsf3
define float @fmul_test(float %a, float %b) {
  %r = fmul float %a, %b
  ret float %r
}

; HW-LABEL: fdiv_test:
; HW: fdiv
; SOFT-LABEL: fdiv_test:
; SOFT: bralid {{.*}}, __divsf3
define float @fdiv_test(float %a, float %b) {
  %r = fdiv float %a, %b
  ret float %r
}

; HW-LABEL: fsqrt_test:
; HW: fsqrt
define float @fsqrt_test(float %a) {
  %r = call float @llvm.sqrt.f32(float %a)
  ret float %r
}

; HW-LABEL: sint_to_fp_test:
; HW: flt
define float @sint_to_fp_test(i32 %a) {
  %r = sitofp i32 %a to float
  ret float %r
}

; HW-LABEL: fp_to_sint_test:
; HW: fint
define i32 @fp_to_sint_test(float %a) {
  %r = fptosi float %a to i32
  ret i32 %r
}

declare float @llvm.sqrt.f32(float)
