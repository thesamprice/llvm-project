; NOTE: Do not autogenerate
; Indirect function calls must use brald (branch-and-link to register),
; not bralid (branch-and-link to immediate/label). Per UG984 Ch.5:
;   brald rD, rB  — PC := PC + rB; rD := PC + 4 (delay slot)
;
; RUN: llc -mtriple=microblazeel-unknown-elf < %s | FileCheck %s
; REQUIRES: microblaze-registered-target
; Simple arithmetic functions used as indirect-call targets.

define i32 @do_add(i32 %a, i32 %b) {
; CHECK-LABEL: do_add:
; CHECK:       rtsd r15, 8
; CHECK:       addk r3, r5, r6
    %r = add i32 %a, %b
    ret i32 %r
}

define i32 @do_sub(i32 %a, i32 %b) {
; CHECK-LABEL: do_sub:
; CHECK:       rtsd r15, 8
; CHECK:       rsubk r3, r6, r5
    %r = sub i32 %a, %b
    ret i32 %r
}

; Indirect call via a function pointer argument.
; Must emit brald (branch-and-link to register), not bralid (which needs a
; known label). brald r15, rB: rB holds the target address, r15 gets PC+8.

define i32 @call_through_ptr(ptr %fn, i32 %a, i32 %b) {
; CHECK-LABEL:  call_through_ptr:
; CHECK:        brald r15, {{r[0-9]+}}
; CHECK-NOT:    bralid
    %r = call i32 %fn(i32 %a, i32 %b)
    ret i32 %r
}

; Indirect call through a table entry. A shift libcall (bralid __ashlsi3)
; may appear before the brald for the actual indirect call — that is expected.

@FNTABLE = private constant [2 x ptr] [ ptr @do_add, ptr @do_sub ]

define i32 @call_table_entry(i32 %a, i32 %b, i32 %idx) {
; CHECK-LABEL:  call_table_entry:
; CHECK:        brald r15, {{r[0-9]+}}
    %slot = getelementptr [2 x ptr], ptr @FNTABLE, i32 0, i32 %idx
    %fn = load ptr, ptr %slot
    %r = call i32 %fn(i32 %a, i32 %b)
    ret i32 %r
}
