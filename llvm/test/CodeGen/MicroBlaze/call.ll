; RUN: llc -mtriple=microblazeel < %s | FileCheck %s

declare i32 @ext(i32, i32)

; Leaf function: no frame manipulation.
; CHECK-LABEL: leaf:
; CHECK-NOT: addik r1
; CHECK: addk r3, r5, r6
; CHECK: rtsd r15, 8
define i32 @leaf(i32 %a, i32 %b) {
  %r = add i32 %a, %b
  ret i32 %r
}

; Non-leaf: must save/restore r15.
; CHECK-LABEL: caller:
; CHECK: addik r1, r1, -4
; CHECK: swi r15, r1, 0
; CHECK: bralid r15, ext
; CHECK: lwi r15, r1, 0
; CHECK: addik r1, r1, 4
; CHECK: rtsd r15, 8
define i32 @caller(i32 %a, i32 %b) {
  %r = call i32 @ext(i32 %a, i32 %b)
  ret i32 %r
}

; When a value must survive a call, it is saved in a callee-saved register (r19),
; which is then spilled to the frame.
; CHECK-LABEL: six_args:
; CHECK: swi r15, r1
; CHECK: swi r19, r1
; CHECK: bralid r15, ext
; CHECK: lwi r19, r1
; CHECK: lwi r15, r1
; CHECK: rtsd r15, 8
define i32 @six_args(i32 %a, i32 %b, i32 %c, i32 %d, i32 %e, i32 %f) {
  %r = call i32 @ext(i32 %a, i32 %b)
  %s = add i32 %r, %c
  ret i32 %s
}

; Callee-saved register r19 must be spilled when live across a call.
; CHECK-LABEL: caller_many:
; CHECK: swi r19, r1
; CHECK: bralid r15, ext
; CHECK: lwi r19, r1
define i32 @caller_many(i32 %a, i32 %b, i32 %c, i32 %d, i32 %e, i32 %f, i32 %g) {
  %r = call i32 @ext(i32 %a, i32 %b)
  %s = add i32 %r, %c
  ret i32 %s
}
