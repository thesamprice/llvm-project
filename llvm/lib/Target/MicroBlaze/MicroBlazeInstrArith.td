//===-- MicroBlazeInstrArith.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// All instructions in this file read or write MSR.C (the carry bit), which is
// modeled as the physical MSR_C register (see MicroBlazeRegisterInfo.td).
// Per-instruction Defs/Uses declare the MSR_C liveness, eliminating the need
// for hasSideEffects=1 to prevent carry-consumer reordering.
// For ordinary addition and subtraction use ADDK/RSUBK/ADDIK/RSUBIK
// (MicroBlazeInstrALU.td), which do not touch MSR.C.
//
// Carry-chain DAG nodes (MicroBlazeISD::ADDC/ADDE/SUBC/SUBE) are wired to
// these instructions via Pat entries at the bottom of this file.  The physical
// MSR_C register flows between nodes the same way ARM uses CPSR, enabling the
// UADDO_CARRY/USUBO_CARRY Custom lowering to produce 2-instruction i64 chains
// after the round-trip combine fires (see MicroBlazeISelLowering.cpp).

//===----------------------------------------------------------------------===//
// Carry-arithmetic Type A — ADD/RSUB variants that read/write MSR.C
// UG984 §5 (Figs 59): opcode bits[28,27] = K,C  bit[26]=0 add  bit[26]=1 rsub
// Latency: 1 cycle.
//===----------------------------------------------------------------------===//

// ADD Rd, Ra, Rb  — Rd = Ra + Rb; writes MSR.C  (opcode 0x00, K=0 C=0).
def ADD    : MBA<0x00, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                 "add\t$rD, $rA, $rB",
                 [(set GPR:$rD, MSR_C, (MBaddc GPR:$rA, GPR:$rB))]> {
  let Defs = [MSR_C];
}
// ADDC Rd, Ra, Rb — Rd = Ra + Rb + MSR.C; reads and writes MSR.C  (opcode 0x02).
def ADDC   : MBA<0x02, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                 "addc\t$rD, $rA, $rB",
                 [(set GPR:$rD, MSR_C, (MBadde GPR:$rA, GPR:$rB, MSR_C))]> {
  let Defs = [MSR_C];
  let Uses = [MSR_C];
}
// ADDKC Rd, Ra, Rb — Rd = Ra + Rb + MSR.C; reads but keeps MSR.C  (opcode 0x06).
def ADDKC  : MBA<0x06, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                 "addkc\t$rD, $rA, $rB", []> {
  let Uses = [MSR_C];
}
// RSUB Rd, Ra, Rb  — Rd = Rb - Ra; writes MSR.C  (opcode 0x01, K=0 C=0).
// Note: RSUB encoding is rD = rB - rA, so DAG subc(lhs,rhs) → RSUB(rA=rhs,rB=lhs).
def RSUB   : MBA<0x01, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                 "rsub\t$rD, $rA, $rB", []> {
  let Defs = [MSR_C];
}
// RSUBC Rd, Ra, Rb — Rd = Rb - Ra + MSR.C - 1; reads and writes MSR.C  (opcode 0x03).
def RSUBC  : MBA<0x03, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                 "rsubc\t$rD, $rA, $rB", []> {
  let Defs = [MSR_C];
  let Uses = [MSR_C];
}
// RSUBKC Rd, Ra, Rb — reverse subtract w/ carry-in; keeps MSR.C  (opcode 0x07).
def RSUBKC : MBA<0x07, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                 "rsubkc\t$rD, $rA, $rB", []> {
  let Uses = [MSR_C];
}

//===----------------------------------------------------------------------===//
// Carry-arithmetic Type B — ADDI/RSUBI variants  (UG984 §5 Fig 60).
// Latency: 1 cycle.
//===----------------------------------------------------------------------===//

// ADDI Rd, Ra, imm  — Rd = Ra + sext(imm); writes MSR.C  (opcode 0x08).
def ADDI    : MBB<0x08, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                  "addi\t$rD, $rA, $imm", []> {
  let Defs = [MSR_C];
}
// ADDIC Rd, Ra, imm — Rd = Ra + sext(imm) + MSR.C; reads and writes MSR.C  (opcode 0x0A).
def ADDIC   : MBB<0x0A, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                  "addic\t$rD, $rA, $imm", []> {
  let Defs = [MSR_C];
  let Uses = [MSR_C];
}
// ADDIKC Rd, Ra, imm — Rd = Ra + sext(imm) + MSR.C; keeps MSR.C  (opcode 0x0E).
def ADDIKC  : MBB<0x0E, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                  "addikc\t$rD, $rA, $imm", []> {
  let Uses = [MSR_C];
}
// RSUBI Rd, Ra, imm  — Rd = sext(imm) - Ra; writes MSR.C  (opcode 0x09).
def RSUBI   : MBB<0x09, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                  "rsubi\t$rD, $rA, $imm", []> {
  let Defs = [MSR_C];
}
// RSUBIC Rd, Ra, imm — Rd = sext(imm) - Ra + MSR.C - 1; reads and writes MSR.C  (opcode 0x0B).
def RSUBIC  : MBB<0x0B, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                  "rsubic\t$rD, $rA, $imm", []> {
  let Defs = [MSR_C];
  let Uses = [MSR_C];
}
// RSUBIKC Rd, Ra, imm — reverse subtract imm w/ carry-in; keeps MSR.C  (opcode 0x0F).
def RSUBIKC : MBB<0x0F, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                  "rsubikc\t$rD, $rA, $imm", []> {
  let Uses = [MSR_C];
}

// ANDNI Rd, Ra, imm — Rd = Ra & ~sext(imm)  (opcode 0x2B).  UG984 §5 Fig 64. 1 cycle.
// Pure bitwise operation; no MSR access.
def ANDNI   : MBB<0x2B, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                  "andni\t$rD, $rA, $imm", []>;

//===----------------------------------------------------------------------===//
// Carry-chain SelectionDAG patterns
//
// MicroBlaze carry convention: MSR.C = 1 means no-borrow / carry-out set.
// MSR_C is the physical carry register; it flows between nodes as an i32
// DAG value the same way ARM uses CPSR.
//
// MicroBlazeISD::ADDC(a,b)      → ADD   rD,rA,rB   — a+b, writes MSR_C
// MicroBlazeISD::ADDE(a,b,MSR_C)→ ADDC  rD,rA,rB   — a+b+carry, rw MSR_C
// MicroBlazeISD::SUBC(a,b)      → RSUB  rD,rhs,lhs — a-b, writes MSR_C
// MicroBlazeISD::SUBE(a,b,MSR_C)→ RSUBC rD,rhs,lhs — a-b+carry-1, rw MSR_C
//
// ADD/ADDC patterns are inline (see instruction defs above).
// RSUB/RSUBC use separate Pat<> because the operands are swapped
// (rD = rB - rA, so subc(lhs,rhs) → RSUB(rA=rhs, rB=lhs)).
//===----------------------------------------------------------------------===//

// RSUB/RSUBC operands are reversed (rD = rB - rA), so subc(lhs,rhs) maps to
// RSUB(rA=rhs, rB=lhs).  The instruction's Defs=[MSR_C] handles the implicit
// second output of MBsubc/MBsube, same as the inline patterns on ADD/ADDC above.
def : Pat<(MBsubc GPR:$lhs, GPR:$rhs), (RSUB  GPR:$rhs, GPR:$lhs)>;
def : Pat<(MBsube GPR:$lhs, GPR:$rhs, MSR_C), (RSUBC GPR:$rhs, GPR:$lhs)>;
