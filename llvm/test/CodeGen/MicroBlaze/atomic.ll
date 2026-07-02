; RUN: llc -mtriple=microblazeel-unknown-linux-gnu %s -o - | FileCheck %s
;
; Verify that cmpxchg and atomicrmw lower to LWX/SWX LL/SC loops.
; LWX sets the hardware reservation; SWX stores conditionally and sets
; MSR[C]=0 on success / MSR[C]=1 on failure (UG984 §5, opcodes 0x32/0x36).

; CHECK-LABEL: test_cmpxchg:
; CHECK:       lwx {{r[0-9]+}}, {{r[0-9]+}}, r0
; CHECK:       swx {{r[0-9]+}}, {{r[0-9]+}}, r0
; CHECK:       mfs {{r[0-9]+}}, rmsr
; CHECK:       andi {{r[0-9]+}}, {{r[0-9]+}}, 4
define i32 @test_cmpxchg(ptr %p, i32 %expected, i32 %desired) {
entry:
  %res = cmpxchg ptr %p, i32 %expected, i32 %desired seq_cst seq_cst
  %val = extractvalue { i32, i1 } %res, 0
  ret i32 %val
}

; CHECK-LABEL: test_cmpxchg_bool:
; CHECK:       lwx {{r[0-9]+}}, {{r[0-9]+}}, r0
; CHECK:       swx {{r[0-9]+}}, {{r[0-9]+}}, r0
; CHECK:       mfs {{r[0-9]+}}, rmsr
; CHECK:       andi {{r[0-9]+}}, {{r[0-9]+}}, 4
define i1 @test_cmpxchg_bool(ptr %p, i32 %expected, i32 %desired) {
entry:
  %res = cmpxchg ptr %p, i32 %expected, i32 %desired seq_cst seq_cst
  %ok = extractvalue { i32, i1 } %res, 1
  ret i1 %ok
}

; CHECK-LABEL: test_atomicrmw_add:
; CHECK:       lwx {{r[0-9]+}}, {{r[0-9]+}}, r0
; CHECK:       swx {{r[0-9]+}}, {{r[0-9]+}}, r0
; CHECK:       mfs {{r[0-9]+}}, rmsr
; CHECK:       andi {{r[0-9]+}}, {{r[0-9]+}}, 4
define i32 @test_atomicrmw_add(ptr %p, i32 %val) {
entry:
  %old = atomicrmw add ptr %p, i32 %val seq_cst
  ret i32 %old
}

; CHECK-LABEL: test_atomicrmw_xchg:
; CHECK:       lwx {{r[0-9]+}}, {{r[0-9]+}}, r0
; CHECK:       swx {{r[0-9]+}}, {{r[0-9]+}}, r0
; CHECK:       mfs {{r[0-9]+}}, rmsr
; CHECK:       andi {{r[0-9]+}}, {{r[0-9]+}}, 4
define i32 @test_atomicrmw_xchg(ptr %p, i32 %val) {
entry:
  %old = atomicrmw xchg ptr %p, i32 %val seq_cst
  ret i32 %old
}
