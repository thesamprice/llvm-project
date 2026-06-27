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

// MBAR imm — memory barrier; imm selects barrier type  (opcode 0x2E, rD=0b00010).
// UG984 §5: imm[0]=sync data side, imm[1]=sync instruction side.
def MBAR : MBB<0x2E, (outs), (ins simm16:$imm), "mbar\t$imm", []> {
  let rD = 0b00010;
  let rA = 0b00000;
  let hasSideEffects = 1;
}
