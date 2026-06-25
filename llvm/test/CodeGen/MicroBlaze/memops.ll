; RUN: llc -mtriple=microblazeel < %s | FileCheck %s
; REQUIRES: microblaze-registered-target

; i8 and i16 memory operations.

; --- Loads ---

; CHECK-LABEL: load_i8_zext:
; CHECK: lbui r3, r5, 0
define i32 @load_i8_zext(ptr %p) {
  %v = load i8, ptr %p
  %r = zext i8 %v to i32
  ret i32 %r
}

; CHECK-LABEL: load_i8_sext:
; CHECK: lbui r3, r5, 0
; CHECK: sext8 r3, r3
define i32 @load_i8_sext(ptr %p) {
  %v = load i8, ptr %p
  %r = sext i8 %v to i32
  ret i32 %r
}

; CHECK-LABEL: load_i16_zext:
; CHECK: lhui r3, r5, 0
define i32 @load_i16_zext(ptr %p) {
  %v = load i16, ptr %p
  %r = zext i16 %v to i32
  ret i32 %r
}

; CHECK-LABEL: load_i16_sext:
; CHECK: lhui r3, r5, 0
; CHECK: sext16 r3, r3
define i32 @load_i16_sext(ptr %p) {
  %v = load i16, ptr %p
  %r = sext i16 %v to i32
  ret i32 %r
}

; CHECK-LABEL: load_i32:
; CHECK: lwi r3, r5, 0
define i32 @load_i32(ptr %p) {
  %r = load i32, ptr %p
  ret i32 %r
}

; Load with immediate offset
; CHECK-LABEL: load_i32_offset:
; CHECK: lwi r3, r5, 8
define i32 @load_i32_offset(ptr %p) {
  %q = getelementptr i32, ptr %p, i32 2
  %r = load i32, ptr %q
  ret i32 %r
}

; --- Stores ---

; CHECK-LABEL: store_i8:
; CHECK: sbi r6, r5, 0
define void @store_i8(ptr %p, i8 %v) {
  store i8 %v, ptr %p
  ret void
}

; CHECK-LABEL: store_i16:
; CHECK: shi r6, r5, 0
define void @store_i16(ptr %p, i16 %v) {
  store i16 %v, ptr %p
  ret void
}

; CHECK-LABEL: store_i32:
; CHECK: swi r6, r5, 0
define void @store_i32(ptr %p, i32 %v) {
  store i32 %v, ptr %p
  ret void
}
