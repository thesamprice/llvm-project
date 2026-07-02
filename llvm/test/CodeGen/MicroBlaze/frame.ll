; RUN: llc -mtriple=microblazeel < %s | FileCheck %s

declare void @use(ptr)

; Leaf with alloca: SP adjusted, no r15 save needed.
; Delay slot filler hoists 'addik r1, r1, 4' into rtsd's delay slot.
; CHECK-LABEL: with_alloca:
; CHECK: addik r1, r1, -4
; CHECK-NOT: swi r15
; CHECK: rtsd r15, 8
; CHECK: addik r1, r1, 4
define i32 @with_alloca(i32 %x) {
  %buf = alloca i32
  store i32 %x, ptr %buf
  %v = load i32, ptr %buf
  ret i32 %v
}

; Non-leaf with alloca: SP adjusted, r15 saved, alloca addr passed to callee.
; Delay slot filler hoists 'addik r1, r1, 8' into rtsd's delay slot.
; CHECK-LABEL: with_call_and_local:
; CHECK: addik r1, r1, -8
; CHECK: swi r15, r1, 0
; CHECK: swi r5, r1, 4
; CHECK: addik r5, r1, 4
; CHECK: bralid r15, use
; CHECK: lwi r15, r1, 0
; CHECK: rtsd r15, 8
; CHECK: addik r1, r1, 8
define void @with_call_and_local(i32 %x) {
  %buf = alloca i32
  store i32 %x, ptr %buf
  call void @use(ptr %buf)
  ret void
}

; Global address materialization: addik rN, r0, symbol.
; CHECK-LABEL: read_global:
; CHECK: addik {{r[0-9]+}}, r0, g
; CHECK: lwi r3
define i32 @read_global() {
  %v = load i32, ptr @g
  ret i32 %v
}

@g = global i32 0
