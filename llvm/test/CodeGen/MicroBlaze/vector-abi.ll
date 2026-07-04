; RUN: llc -mtriple=microblazeel -verify-machineinstrs < %s | FileCheck %s

; MicroBlaze has no SIMD hardware.  The Clang ABI front-end coerces vector
; arguments and return values to integer words before reaching the backend:
;   ≤4 bytes  → i32 (fits in one register)
;   ≤8 bytes  → [2 x i32] (fits in R3:R4 / R5:R6)
;   >8 bytes  → sret pointer (return) or byval copy (argument)
;
; These tests use the coerced IR that the Clang ABI layer produces for C
; vector types.  The backend itself sees only integer types.

; --- 4-byte vector (v4i8) ---
; Clang ABI: coerced to i32.  Arg in R5, returned in R3.
; CHECK-LABEL: vec4_pass:
define i32 @vec4_pass(i32 %0) nounwind {
  ret i32 %0
}
; CHECK: rtsd r15, 8
; CHECK-NEXT: addk r3, r5, r0

; --- 8-byte vector (v8i8) ---
; Clang ABI: coerced to [2 x i32].  Args in R5:R6, returned in R3:R4.
; CHECK-LABEL: vec8_pass:
define [2 x i32] @vec8_pass([2 x i32] %0) nounwind {
  ret [2 x i32] %0
}
; R3=R5 is in the delay slot; R4=R6 is scheduled before the rtsd.
; CHECK: addk r4, r6, r0
; CHECK: rtsd r15, 8
; CHECK-NEXT: addk r3, r5, r0

; --- 16-byte vector (v4i32) ---
; Too large for R3:R4 → returned via hidden sret pointer in R5; args in R6–R9.
; CHECK-LABEL: vec16_pass:
define void @vec16_pass(ptr sret([4 x i32]) align 4 %agg.result,
                        i32 %0, i32 %1, i32 %2, i32 %3) nounwind {
  %p = getelementptr inbounds [4 x i32], ptr %agg.result, i32 0, i32 0
  store i32 %0, ptr %p, align 4
  %p1 = getelementptr inbounds [4 x i32], ptr %agg.result, i32 0, i32 1
  store i32 %1, ptr %p1, align 4
  %p2 = getelementptr inbounds [4 x i32], ptr %agg.result, i32 0, i32 2
  store i32 %2, ptr %p2, align 4
  %p3 = getelementptr inbounds [4 x i32], ptr %agg.result, i32 0, i32 3
  store i32 %3, ptr %p3, align 4
  ret void
}
; Four stores to the sret pointer; no attempt to return a vector in registers.
; CHECK-COUNT-4: swi r{{[0-9]+}}, r5,
; CHECK: rtsd r15, 8
