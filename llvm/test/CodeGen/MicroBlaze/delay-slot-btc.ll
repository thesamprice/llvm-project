; NOTE: Do not autogenerate
; delay-slot-btc.ll — branch-target-cache-aware demote-vs-NOP fallback.
;
; When the delay slot of a backward (loop back-edge) branch cannot be filled,
; the default heuristic keeps the D-form and inserts a NOP: the branch is taken
; almost every iteration, so D+NOP (2 cycles) beats demoting to the non-D form
; (3 cycles taken).
;
; With a branch target cache (+branch-target-cache, UG984 Ch.2 "Branch Target
; Cache") a correctly predicted immediate branch incurs no refill overhead, so
; the D-form saves nothing and the NOP is a wasted cycle every iteration —
; demoting to the no-delay form (bneid -> bnei, no NOP) is strictly better.
;
; The cache does not exist on the 3-stage area pipeline, so +area-optimized
; suppresses the demote even when +branch-target-cache is also given.  Returns
; (rtsd) have no no-delay form and always keep their NOP on every setting.
;
; The loop's only hoist candidate (addik r3, r3, -1) feeds the compare, so it is
; ineligible for the delay slot and the demote-vs-NOP fallback is exercised.
;
; RUN: llc -mtriple=microblazeel -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,NOBTC
; RUN: llc -mtriple=microblazeel -mattr=+branch-target-cache -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,BTC
; RUN: llc -mtriple=microblazeel -mattr=+branch-target-cache,+area-optimized \
; RUN:     -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,NOBTC
; REQUIRES: microblaze-registered-target
define void @countdown(ptr %p, i32 %n) {
; CHECK-LABEL: countdown:
; CHECK:       cmp [[C:r[0-9]+]], {{r[0-9]+}}, {{r[0-9]+}}
;
; Default / area (no usable cache): keep the D-form back-edge branch + NOP.
; NOBTC-NEXT:  bneid [[C]], .LBB0_1
; NOBTC-NEXT:  nop
;
; Cache present: demote to the no-delay form, no wasted NOP.
; BTC-NEXT:    bnei [[C]], .LBB0_1
; BTC-NOT:     bneid
;
; The return has no no-delay form: it keeps its NOP on every setting.
; CHECK:       rtsd r15, 8
; CHECK-NEXT:  nop
entry:
  br label %loop
loop:
  %i = phi i32 [ %n, %entry ], [ %i1, %loop ]
  %i1 = add i32 %i, -1
  store i32 %i1, ptr %p
  %c = icmp ne i32 %i1, 0
  br i1 %c, label %loop, label %exit
exit:
  ret void
}
