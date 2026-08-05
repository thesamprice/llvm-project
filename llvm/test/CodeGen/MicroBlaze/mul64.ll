; NOTE: Do not autogenerate
; 64-bit integer multiply lowering.
;
; Without +multiply-high the backend has no way to select umul_lohi/smul_lohi
; (the two 32-bit halves needed to form a 64-bit product), so i64 mul is
; lowered to a __muldi3 libcall.
;
; NOTE: With +multiply-high the backend currently crashes (Cannot select
; umul_lohi) because mulh/mulhu ISel patterns for the i64 case are not yet
; wired up. That path is not tested here.
;
; RUN: llc -mtriple=microblazeel-unknown-elf < %s | FileCheck %s
; REQUIRES: microblaze-registered-target
define i64 @mul_i64_soft(i64 %a, i64 %b) {
; i64 args arrive in R5:R6 (low:high of %a) and R7:R8 (low:high of %b).
; The libcall preserves this layout.
; CHECK-LABEL: mul_i64_soft:
; CHECK-NOT:   mulh
; CHECK:       bralid r15, __muldi3
; CHECK:       rtsd r15, 8
    %r = mul i64 %a, %b
    ret i64 %r
}
