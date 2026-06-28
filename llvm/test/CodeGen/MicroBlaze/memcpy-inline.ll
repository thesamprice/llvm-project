; NOTE: Do not autogenerate
; RUN: llc -mtriple=microblazeel-unknown-elf -mattr=+barrel-shift,+pattern-compare,+multiply-high \
; RUN:     -O2 < %s | FileCheck %s
; REQUIRES: microblaze-registered-target;
; MaxStoresPerMemcpy=16 (64 bytes) — struct copies up to 64 bytes must inline
; as lwi/swi sequences rather than calling memcpy.

target datalayout = "e-m:e-p:32:32-i8:8:32-i16:16:32-i64:32-f64:32-n32"
target triple = "microblazeel-unknown-unknown-elf"

; 48-byte struct copy (12 i32 words) — covers dhrystone's Rec_Type.
; CHECK-LABEL: copy48:
; CHECK-NOT: bralid {{.*}}, memcpy
; CHECK: lwi
; CHECK: swi
define void @copy48(ptr %dst, ptr %src) {
  call void @llvm.memcpy.p0.p0.i32(ptr align 4 %dst, ptr align 4 %src, i32 48, i1 false)
  ret void
}

; 64-byte struct copy (16 i32 words) — exactly at threshold, still inlined.
; CHECK-LABEL: copy64:
; CHECK-NOT: bralid {{.*}}, memcpy
; CHECK: lwi
; CHECK: swi
define void @copy64(ptr %dst, ptr %src) {
  call void @llvm.memcpy.p0.p0.i32(ptr align 4 %dst, ptr align 4 %src, i32 64, i1 false)
  ret void
}

; 68-byte copy (17 words) — one word over threshold, must call memcpy.
; CHECK-LABEL: copy68:
; CHECK: bralid {{r[0-9]+}}, memcpy
define void @copy68(ptr %dst, ptr %src) {
  call void @llvm.memcpy.p0.p0.i32(ptr align 4 %dst, ptr align 4 %src, i32 68, i1 false)
  ret void
}

declare void @llvm.memcpy.p0.p0.i32(ptr, ptr, i32, i1)
