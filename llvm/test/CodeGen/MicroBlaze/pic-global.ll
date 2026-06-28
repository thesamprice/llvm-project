; RUN: llc -mtriple=microblazeel-unknown-elf -relocation-model=pic -O1 < %s | FileCheck %s --check-prefix=PIC
; RUN: llc -mtriple=microblazeel-unknown-elf -relocation-model=static -O1 < %s | FileCheck %s --check-prefix=STATIC
; REQUIRES: microblaze-registered-target

; In PIC mode, global variable addresses come from the GOT via R20 (the GOT base register).
; In static mode, global addresses are materialised directly with addik.

@extern_int = external global i32
@extern_b   = external global i32

; PIC-LABEL:    get_global:
; PIC:          la_got {{r[0-9]+}}, extern_int@got
; PIC-NEXT:     lwi {{r[0-9]+}}, {{r[0-9]+}}, 0
; STATIC-LABEL: get_global:
; STATIC:       addik {{r[0-9]+}}, r0, extern_int
; STATIC-NEXT:  lwi {{r[0-9]+}}, {{r[0-9]+}}, 0
define i32 @get_global() {
  %v = load i32, ptr @extern_int, align 4
  ret i32 %v
}

; PIC-LABEL: sum_globals:
; PIC:       la_got {{r[0-9]+}}, {{extern_int|extern_b}}@got
; PIC:       la_got {{r[0-9]+}}, {{extern_int|extern_b}}@got
; STATIC-LABEL: sum_globals:
; STATIC:    addik {{r[0-9]+}}, r0, {{extern_int|extern_b}}
; STATIC:    addik {{r[0-9]+}}, r0, {{extern_int|extern_b}}
define i32 @sum_globals() {
  %a = load i32, ptr @extern_int, align 4
  %b = load i32, ptr @extern_b, align 4
  %r = add i32 %a, %b
  ret i32 %r
}
