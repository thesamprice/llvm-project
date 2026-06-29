; RUN: llc -mtriple=microblazeel -verify-machineinstrs %s -o - | FileCheck %s
; REQUIRES: microblaze-registered-target

; Tests for MicroBlazeDelaySlotFiller::searchJoinBB, which fills a delayed
; branch's slot by MOVING the first safe instruction out of the branch's "join"
; block (the common successor reached on every path).  Because the instruction
; is moved (not cloned), the move is only valid when:
;   * the join block is entered exclusively through the branch's delay slot, and
;   * the moved instruction does not depend on (RAW) or race (WAW) a value an
;     intermediate block writes, nor clobber (WAR) a value it reads.

declare i32 @llvm.abs.i32(i32, i1)

; ── Positive: searchJoinBB fills a bgtid slot ───────────────────────────────
; In the abs diamond inside the loop, the independent induction-variable bump
; (addik i, i, 1) is the first safe instruction of the join block and is moved
; into the bgtid delay slot.  The negation block only touches the loaded value,
; so moving the bump is safe.
;
; CHECK-LABEL: sum_abs:
; CHECK:      lwi [[V:r[0-9]+]], {{r[0-9]+}}, 0
; CHECK-NEXT: bgtid [[V]], .LBB
; CHECK-NEXT: addik [[I:r[0-9]+]], [[I]], 1
; CHECK:      rsubk [[V]], [[V]], r0
define i32 @sum_abs(ptr %a, i32 %n) {
entry:
  %z = icmp sgt i32 %n, 0
  br i1 %z, label %loop, label %done
loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %loop ]
  %acc = phi i32 [ 0, %entry ], [ %acc.next, %loop ]
  %p = getelementptr i32, ptr %a, i32 %i
  %v = load i32, ptr %p
  %av = call i32 @llvm.abs.i32(i32 %v, i1 false)
  %acc.next = add i32 %acc, %av
  %i.next = add i32 %i, 1
  %c = icmp slt i32 %i.next, %n
  br i1 %c, label %loop, label %done
done:
  %r = phi i32 [ 0, %entry ], [ %acc.next, %loop ]
  ret i32 %r
}

; ── Negative (RAW): join instr reads a value the intermediate block writes ───
; if (c) y = a*a; z = y + 7.  The join's "y + 7" reads y, which the then-block
; writes.  Moving it into the conditional branch's delay slot would read the
; stale y on the fall-through path, so searchJoinBB must refuse: the branch is
; downgraded to the non-delayed form and is immediately followed by the
; unconditional branch (NOT the dependent add).
;
; CHECK-LABEL: tri_raw:
; CHECK:      blti r3, .LBB
; CHECK-NEXT: bri .LBB
define i32 @tri_raw(i32 %c, i32 %a, i32 %y0) {
entry:
  %cond = icmp sgt i32 %c, 0
  br i1 %cond, label %join, label %then
then:
  %y1 = mul i32 %a, %a
  br label %join
join:
  %y = phi i32 [ %y0, %entry ], [ %y1, %then ]
  %z = add i32 %y, 7
  ret i32 %z
}

; ── Negative (memory RAW): join loads from a stack slot an intermediate block stores ─
; if (c) mem[sp+k] = new_val; result = mem[sp+k] + 7.
; The join's load reads the slot that the then-block writes.  Hoisting the load
; into the delay slot would read the PRE-store value on the taken path, so
; searchJoinBB must refuse (InterHasStore guard): branch stays non-delayed.
; The backend lays out then before join here, so the branch fires for the
; join path (c>0): blti (fires when r3<0, i.e., c>0) not bltid.
;
; CHECK-LABEL: tri_mem_raw:
; CHECK:      blti r3, .LBB
; CHECK-NOT:  bltid
define i32 @tri_mem_raw(i32 %c, i32 %new_val) {
entry:
  %slot = alloca i32, align 4
  store i32 42, ptr %slot
  %cond = icmp sgt i32 %c, 0
  br i1 %cond, label %join, label %then
then:
  store i32 %new_val, ptr %slot
  br label %join
join:
  %v = load i32, ptr %slot
  %z = add i32 %v, 7
  ret i32 %z
}

; ── Negative (extra predecessor): join is reachable from outside the diamond ─
; %join has a third predecessor (%other) that does not run the branch's delay
; slot, so moving the join instruction would skip it on that path.  Again the
; branch must stay non-delayed with bri following it.
;
; CHECK-LABEL: tri_extra_pred:
; CHECK:      blti r3, .LBB
; CHECK-NEXT: bri .LBB
define i32 @tri_extra_pred(i32 %c, i32 %a, i32 %s, i32 %sel) {
entry:
  %u = icmp eq i32 %sel, 0
  br i1 %u, label %other, label %head
head:
  %cond = icmp sgt i32 %c, 0
  br i1 %cond, label %join, label %then
then:
  %t = add i32 %a, 1
  br label %join
other:
  br label %join
join:
  %y = phi i32 [ %a, %head ], [ %t, %then ], [ %s, %other ]
  %z = mul i32 %y, %y
  ret i32 %z
}
