//===-- MicroBlazeInstrReturns.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Special return instructions (Type B, opcode 0x2D) and memory barrier
// UG984 §5: rD field encodes the return type.  Latency: 1 cycle.
//
// rD  Return type
// 0b10000  rtsd — subroutine  (already defined in instrinfo-branch)
// 0b10001  rtid — interrupt
// 0b10010  rtbd — break
// 0b10100  rted — exception
//===----------------------------------------------------------------------===//

// RTID/RTBD/RTED restore processor state from special registers (MSR, BTR,
// EAR, ESR…) that are not modeled as physical registers.  hasSideEffects=1
// captures those implicit state changes.
let hasSideEffects = 1 in {

// RTID Ra, imm — return from interrupt; resumes at Ra + sext(imm).
def RTID : MBB<0x2D, (outs), (ins GPR:$rA, simm16:$imm),
               "rtid\t$rA, $imm", []> {
  let rD = 0b10001;
  let isReturn = 1; let isTerminator = 1; let isBarrier = 1; let hasDelaySlot = 1;
}

// RTBD Ra, imm — return from break.
def RTBD : MBB<0x2D, (outs), (ins GPR:$rA, simm16:$imm),
               "rtbd\t$rA, $imm", []> {
  let rD = 0b10010;
  let isReturn = 1; let isTerminator = 1; let isBarrier = 1; let hasDelaySlot = 1;
}

// RTED Ra, imm — return from exception.
def RTED : MBB<0x2D, (outs), (ins GPR:$rA, simm16:$imm),
               "rted\t$rA, $imm", []> {
  let rD = 0b10100;
  let isReturn = 1; let isTerminator = 1; let isBarrier = 1; let hasDelaySlot = 1;
}

} // let hasSideEffects = 1

// MBAR imm — memory barrier (opcode 0x2E, rD=0b00010, rA=0).  UG984 §2, §5.
// imm selects the barrier scope:
//   mbar 0 — full barrier; also clears the Branch Target Cache (BTC). Use only
//             when a BTC flush is explicitly desired; prefer mbar 1 for ordinary
//             data fences to avoid disrupting branch prediction performance.
//   mbar 1 — data-side barrier (imm[0]=1): drains all pending data-memory
//             operations before allowing subsequent memory accesses to proceed.
//             Correct choice for acquire/release/seq_cst fences in C11 atomics.
//   mbar 2 — instruction-side barrier (imm[1]=1): invalidates instruction cache
//             lines in flight; needed after self-modifying code or JIT emission.
//   mbar 3 — data + instruction barrier (imm[0]=1, imm[1]=1), no BTC flush.
//   mbar 4 — synchronizing branch (equivalent to BRI 4); also clears the BTC.
//
// BTC-after-branch hazard (UG984 §2):
//   If mbar is placed immediately after any branch instruction in the physical
//   instruction stream (including when the branch is taken), the BTC is cleared
//   regardless of the immediate value.  This is a performance hazard, not a
//   correctness issue: avoid placing mbar 0 or mbar 4 (BTC-clearing forms) in
//   positions immediately following branch opcodes or their delay slots.
//
// hasSideEffects=1 serves two purposes: it models the hardware memory-barrier
// side effects, and it sets MCID::UnmodeledSideEffects so that the delay-slot
// filler's hasUnmodeledSideEffects() check correctly excludes MBAR from being
// placed in a branch delay slot (UG984 §2 forbids only IMM, branch, and break
// in delay slots; MBAR is technically allowed, but placing a fence in a delay
// slot is semantically pointless and complicates reasoning).
def MBAR : MBB<0x2E, (outs), (ins simm16:$imm), "mbar\t$imm", []> {
  let rD = 0b00010;
  let rA = 0b00000;
  let hasSideEffects = 1;
  let Itinerary = IIC_MBAR;
}
