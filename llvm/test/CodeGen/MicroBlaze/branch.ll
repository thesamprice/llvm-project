; RUN: llc -mtriple=microblazeel < %s | FileCheck %s

; max(a, b): uses cmp + bgeid (branch-if->=0 in delay slot).
; cmp rD,rA,rB sets bit31=1 when rA>rB; bgeid branches when rD>=0 (i.e. a<=b).
; CHECK-LABEL: max:
; CHECK: cmp r3, r5, r6
; CHECK: bgeid r3
; CHECK: bri
; CHECK: rtsd r15, 8
; CHECK: rtsd r15, 8
define i32 @max(i32 %a, i32 %b) {
  %cmp = icmp sgt i32 %a, %b
  br i1 %cmp, label %then, label %else
then:
  ret i32 %a
else:
  ret i32 %b
}

; min(a, b): uses cmp + bleid (branch-if-<=0).
; CHECK-LABEL: min:
; CHECK: cmp r3, r5, r6
; CHECK: bleid r3
; CHECK: bri
; CHECK: rtsd r15, 8
; CHECK: rtsd r15, 8
define i32 @min(i32 %a, i32 %b) {
  %cmp = icmp slt i32 %a, %b
  br i1 %cmp, label %then, label %else
then:
  ret i32 %a
else:
  ret i32 %b
}

; Unconditional branch: loop with phi.
; CHECK-LABEL: loop_phi:
; CHECK: bri
define i32 @loop_phi(i32 %n) {
entry:
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %next = add i32 %i, 1
  %cmp = icmp slt i32 %next, %n
  br i1 %cmp, label %loop, label %done
done:
  ret i32 %next
}
