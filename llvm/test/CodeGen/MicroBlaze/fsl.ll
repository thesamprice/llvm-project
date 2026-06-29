; NOTE: Do not autogenerate
; RUN: llc -mtriple=microblazeel -mattr=+fsl < %s | FileCheck %s
; REQUIRES: microblaze-registered-target
; FSL/AXI-Stream intrinsics (UG984 §5).  A constant port (0..15) selects the
; static encoding (get/put rD, rfslN); a register port selects the dynamic
; encoding (getd/putd rD, rB).  Covers a representative spread of flag
; combinations, including a carry-defining N variant.

declare i32 @llvm.microblaze.fsl.get(i32)
declare i32 @llvm.microblaze.fsl.tnecaget(i32)
declare i32 @llvm.microblaze.fsl.nget(i32)
declare void @llvm.microblaze.fsl.put(i32, i32)
declare void @llvm.microblaze.fsl.tnecaput(i32, i32)

; --- Dynamic (register port) ---

; CHECK-LABEL: get_dyn:
; CHECK: getd r3, r5
define i32 @get_dyn(i32 %port) {
  %v = call i32 @llvm.microblaze.fsl.get(i32 %port)
  ret i32 %v
}

; CHECK-LABEL: tnecaget_dyn:
; CHECK: tnecagetd r3, r5
define i32 @tnecaget_dyn(i32 %port) {
  %v = call i32 @llvm.microblaze.fsl.tnecaget(i32 %port)
  ret i32 %v
}

; CHECK-LABEL: put_dyn:
; CHECK: putd r5, r6
define void @put_dyn(i32 %val, i32 %port) {
  call void @llvm.microblaze.fsl.put(i32 %val, i32 %port)
  ret void
}

; CHECK-LABEL: tnecaput_dyn:
; CHECK: tnecaputd r5, r6
define void @tnecaput_dyn(i32 %val, i32 %port) {
  call void @llvm.microblaze.fsl.tnecaput(i32 %val, i32 %port)
  ret void
}

; --- Static (constant port) ---

; CHECK-LABEL: get_static:
; CHECK-NOT: getd
; CHECK: get r3, rfsl0
define i32 @get_static() {
  %v = call i32 @llvm.microblaze.fsl.get(i32 0)
  ret i32 %v
}

; CHECK-LABEL: tnecaget_static:
; CHECK-NOT: tnecagetd
; CHECK: tnecaget r3, rfsl15
define i32 @tnecaget_static() {
  %v = call i32 @llvm.microblaze.fsl.tnecaget(i32 15)
  ret i32 %v
}

; CHECK-LABEL: nget_static:
; CHECK-NOT: ngetd
; CHECK: nget r3, rfsl7
define i32 @nget_static() {
  %v = call i32 @llvm.microblaze.fsl.nget(i32 7)
  ret i32 %v
}

; CHECK-LABEL: put_static:
; CHECK-NOT: putd
; CHECK: put r5, rfsl3
define void @put_static(i32 %val) {
  call void @llvm.microblaze.fsl.put(i32 %val, i32 3)
  ret void
}

; CHECK-LABEL: tnecaput_static:
; CHECK-NOT: tnecaputd
; CHECK: tnecaput r5, rfsl1
define void @tnecaput_static(i32 %val) {
  call void @llvm.microblaze.fsl.tnecaput(i32 %val, i32 1)
  ret void
}

; A constant port outside 0..15 has no static form; it must use the dynamic
; encoding with the port materialized into a register.
; CHECK-LABEL: get_static_oob:
; CHECK: getd r3,
define i32 @get_static_oob() {
  %v = call i32 @llvm.microblaze.fsl.get(i32 20)
  ret i32 %v
}
