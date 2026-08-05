; NOTE: Do not autogenerate
; Calling convention: return value registers, argument registers, and
; stack overflow argument placement.
;
; Per UG984 Ch.4 p.185-189:
;   R3-R4   : return values (i32 in R3; i64 low word in R3, high word in R4)
;   R5-R10  : first 6 integer arguments
;   stack   : arguments 7+ passed on stack by caller
;
; NOTE: Our backend places stack overflow args at callee SP+0, SP+4, ...
; The standard ABI (UG984 p.189) specifies SP+28 for the first overflow
; argument (after 4 bytes for R15 + 24-byte mandatory parameter save area).
; This is a known deviation — tests below document actual backend behavior.
;
; RUN: llc -mtriple=microblazeel-unknown-elf < %s | FileCheck %s
; REQUIRES: microblaze-registered-target
; --- Return value registers ---

define void @ret_void() {
; CHECK-LABEL: ret_void:
; CHECK:       rtsd r15, 8
    ret void
}

define i8 @ret_i8() {
; CHECK-LABEL: ret_i8:
; CHECK:       rtsd r15, 8
; CHECK:       addik r3, r0, 1
    ret i8 1
}

define i16 @ret_i16() {
; CHECK-LABEL: ret_i16:
; CHECK:       rtsd r15, 8
; CHECK:       addik r3, r0, 1
    ret i16 1
}

define i32 @ret_i32() {
; CHECK-LABEL: ret_i32:
; CHECK:       rtsd r15, 8
; CHECK:       addik r3, r0, 1
    ret i32 1
}

; i64 returns: low word in R3, high word in R4.
define i64 @ret_i64() {
; CHECK-LABEL: ret_i64:
; CHECK:       addik r3, r0, 1
; CHECK:       rtsd r15, 8
; CHECK:       addik r4, r0, 0
    ret i64 1
}

; --- Argument registers R5-R10 ---

define i32 @arg1(i32 %a) {
; CHECK-LABEL: arg1:
; CHECK:       rtsd r15, 8
; CHECK:       addk r3, r5, r0
    ret i32 %a
}

define i32 @arg2(i32 %a, i32 %b) {
; CHECK-LABEL: arg2:
; CHECK:       rtsd r15, 8
; CHECK:       addk r3, r6, r0
    ret i32 %b
}

define i32 @arg3(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: arg3:
; CHECK:       rtsd r15, 8
; CHECK:       addk r3, r7, r0
    ret i32 %c
}

define i32 @arg4(i32 %a, i32 %b, i32 %c, i32 %d) {
; CHECK-LABEL: arg4:
; CHECK:       rtsd r15, 8
; CHECK:       addk r3, r8, r0
    ret i32 %d
}

define i32 @arg5(i32 %a, i32 %b, i32 %c, i32 %d, i32 %e) {
; CHECK-LABEL: arg5:
; CHECK:       rtsd r15, 8
; CHECK:       addk r3, r9, r0
    ret i32 %e
}

define i32 @arg6(i32 %a, i32 %b, i32 %c, i32 %d, i32 %e, i32 %f) {
; CHECK-LABEL: arg6:
; CHECK:       rtsd r15, 8
; CHECK:       addk r3, r10, r0
    ret i32 %f
}

; --- Stack overflow arguments (callee side) ---
; Argument 7 arrives at callee SP+0 (first slot past the register args).

define i32 @arg7(i32 %a, i32 %b, i32 %c, i32 %d, i32 %e, i32 %f, i32 %g) {
; CHECK-LABEL: arg7:
; CHECK:       rtsd r15, 8
; CHECK:       lwi r3, r1, 0
    ret i32 %g
}

define i32 @arg8(i32 %a, i32 %b, i32 %c, i32 %d, i32 %e, i32 %f,
                 i32 %g, i32 %h) {
; CHECK-LABEL: arg8:
; CHECK:       rtsd r15, 8
; CHECK:       lwi r3, r1, 4
    ret i32 %h
}

define i32 @arg9(i32 %a, i32 %b, i32 %c, i32 %d, i32 %e, i32 %f,
                 i32 %g, i32 %h, i32 %i) {
; CHECK-LABEL: arg9:
; CHECK:       rtsd r15, 8
; CHECK:       lwi r3, r1, 8
    ret i32 %i
}

; --- Stack overflow arguments (caller side) ---
; Caller places arg 7+ starting at its own SP+0 after allocating the frame.

declare i32 @callee7(i32, i32, i32, i32, i32, i32, i32)
declare i32 @callee8(i32, i32, i32, i32, i32, i32, i32, i32)
declare i32 @callee9(i32, i32, i32, i32, i32, i32, i32, i32, i32)

define i32 @call7(i32 %x) {
; CHECK-LABEL: call7:
; CHECK:       swi {{r[0-9]+}}, r1, 0
; CHECK:       addik r5, r0, 1
; CHECK:       addik r6, r0, 2
; CHECK:       addik r7, r0, 3
; CHECK:       addik r8, r0, 4
; CHECK:       addik r9, r0, 5
; CHECK:       bralid r15, callee7
; CHECK:       addik r10, r0, 6
    %r = call i32 @callee7(i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 %x)
    ret i32 %r
}

define i32 @call8(i32 %x) {
; CHECK-LABEL: call8:
; CHECK-DAG:   swi {{r[0-9]+}}, r1, 0
; CHECK-DAG:   swi {{r[0-9]+}}, r1, 4
; CHECK:       bralid r15, callee8
    %r = call i32 @callee8(i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 %x)
    ret i32 %r
}

define i32 @call9(i32 %x) {
; CHECK-LABEL: call9:
; CHECK-DAG:   swi {{r[0-9]+}}, r1, 0
; CHECK-DAG:   swi {{r[0-9]+}}, r1, 4
; CHECK-DAG:   swi {{r[0-9]+}}, r1, 8
; CHECK:       bralid r15, callee9
    %r = call i32 @callee9(i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 %x)
    ret i32 %r
}

; --- Direct calls use bralid ---

declare void @ext()

define void @direct_call() {
; CHECK-LABEL: direct_call:
; CHECK:       bralid r15, ext
    call void @ext()
    ret void
}
