//===-- MicroBlazeInstrArith.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Carry-arithmetic Type A — ADD/RSUB variants that read/write MSR.C
// UG984 §5 (Figs 59): opcode bits[28,27] = K,C  bit[26]=0 add  bit[26]=1 rsub
// Latency: 1 cycle.
//===----------------------------------------------------------------------===//

// ADD Rd, Ra, Rb  — Rd = Ra + Rb; modifies MSR.C  (opcode 0x00, K=0 C=0).
def ADD    : MBA<0x00, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                 "add\t$rD, $rA, $rB", []>;
// ADDC Rd, Ra, Rb — Rd = Ra + Rb + MSR.C; modifies MSR.C  (opcode 0x02, K=0 C=1).
def ADDC   : MBA<0x02, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                 "addc\t$rD, $rA, $rB", []>;
// ADDKC Rd, Ra, Rb — Rd = Ra + Rb + MSR.C; keeps MSR.C  (opcode 0x06, K=1 C=1).
def ADDKC  : MBA<0x06, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                 "addkc\t$rD, $rA, $rB", []>;
// RSUB Rd, Ra, Rb  — Rd = Rb - Ra; modifies MSR.C  (opcode 0x01, K=0 C=0).
def RSUB   : MBA<0x01, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                 "rsub\t$rD, $rA, $rB", []>;
// RSUBC Rd, Ra, Rb — Rd = Rb - Ra + MSR.C - 1; modifies MSR.C  (opcode 0x03).
def RSUBC  : MBA<0x03, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                 "rsubc\t$rD, $rA, $rB", []>;
// RSUBKC Rd, Ra, Rb — reverse subtract w/ carry-in, keeps MSR.C  (opcode 0x07).
def RSUBKC : MBA<0x07, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                 "rsubkc\t$rD, $rA, $rB", []>;

//===----------------------------------------------------------------------===//
// Carry-arithmetic Type B — ADDI/RSUBI variants  (UG984 §5 Fig 60).
// Latency: 1 cycle.
//===----------------------------------------------------------------------===//

// ADDI Rd, Ra, imm  — Rd = Ra + sext(imm); modifies MSR.C  (opcode 0x08).
def ADDI    : MBB<0x08, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                  "addi\t$rD, $rA, $imm", []>;
// ADDIC Rd, Ra, imm — Rd = Ra + sext(imm) + MSR.C; modifies MSR.C  (opcode 0x0A).
def ADDIC   : MBB<0x0A, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                  "addic\t$rD, $rA, $imm", []>;
// ADDIKC Rd, Ra, imm — Rd = Ra + sext(imm) + MSR.C; keeps MSR.C  (opcode 0x0E).
def ADDIKC  : MBB<0x0E, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                  "addikc\t$rD, $rA, $imm", []>;
// RSUBI Rd, Ra, imm  — Rd = sext(imm) - Ra; modifies MSR.C  (opcode 0x09).
def RSUBI   : MBB<0x09, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                  "rsubi\t$rD, $rA, $imm", []>;
// RSUBIC Rd, Ra, imm — Rd = sext(imm) - Ra + MSR.C - 1; modifies MSR.C  (opcode 0x0B).
def RSUBIC  : MBB<0x0B, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                  "rsubic\t$rD, $rA, $imm", []>;
// RSUBIKC Rd, Ra, imm — reverse subtract imm w/ carry-in; keeps MSR.C  (opcode 0x0F).
def RSUBIKC : MBB<0x0F, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                  "rsubikc\t$rD, $rA, $imm", []>;

// ANDNI Rd, Ra, imm — Rd = Ra & ~sext(imm)  (opcode 0x2B).  UG984 §5 Fig 64. 1 cycle.
def ANDNI   : MBB<0x2B, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                  "andni\t$rD, $rA, $imm", []>;
