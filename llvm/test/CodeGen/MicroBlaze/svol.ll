; NOTE: Do not autogenerate
; RUN: llc -mtriple=microblazeel < %s | FileCheck %s
; REQUIRES: microblaze-registered-target

; MicroBlaze save_volatiles calling convention (cc74): an ordinary function that
; additionally preserves the volatile registers (R3-R12) it clobbers, plus the
; dedicated R17/R18 (always).  No MSR is touched and it returns normally (rtsd).
; Ported from the 2013 MBlaze backend (svol.ll).

@.str = private constant [27 x i8] c"The interrupt has gone off\00"

declare i32 @printf(ptr, ...)

; mysvol calls printf, which clobbers the volatile registers, so they are all
; saved/restored along with R17/R18.  Return is the normal rtsd r15, 8.
define cc74 void @mysvol() nounwind noinline {
; CHECK-LABEL: mysvol:
; CHECK:      swi   r3, r1
; CHECK:      swi   r4, r1
; CHECK:      swi   r5, r1
; CHECK:      swi   r6, r1
; CHECK:      swi   r7, r1
; CHECK:      swi   r8, r1
; CHECK:      swi   r9, r1
; CHECK:      swi   r10, r1
; CHECK:      swi   r11, r1
; CHECK:      swi   r12, r1
; CHECK:      swi   r17, r1
; CHECK:      swi   r18, r1
; CHECK-NOT:  mfs   {{r[0-9]+}}, rmsr
entry:
  %call = call i32 (ptr, ...) @printf(ptr @.str)
  ret void
; CHECK-NOT:  mts   rmsr, {{r[0-9]+}}
; CHECK:      lwi   r18, r1
; CHECK:      lwi   r17, r1
; CHECK:      rtsd  r15, 8
}

; mysvol2 clobbers nothing, so only the always-saved R17/R18 appear.
define cc74 void @mysvol2() nounwind noinline {
; CHECK-LABEL: mysvol2:
; CHECK-NOT:  swi   r3, r1
; CHECK-NOT:  swi   r12, r1
; CHECK:      swi   r17, r1
; CHECK:      swi   r18, r1
; CHECK:      lwi   r18, r1
; CHECK:      lwi   r17, r1
; CHECK:      rtsd  r15, 8
entry:
  ret void
}
