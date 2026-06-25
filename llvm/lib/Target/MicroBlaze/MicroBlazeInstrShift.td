//===-- MicroBlazeInstrShift.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Single-bit shift and sign-extend (Type A, opcode 0x24, rB=0)
// UG984 §5: func field selects the operation.  Latency: 1 cycle.
//
// func encodings (11 bits):
//   SRA   0x001 = 0b00000000001  — arithmetic right shift by 1; MSR.C ← bit[0]
//   SRC   0x021 = 0b00000100001  — shift right through carry (MSR.C in/out)
//   SRL   0x041 = 0b01000000001  — logical right shift by 1; MSR.C ← bit[0]
//   SEXT8  0x060 = 0b01100000000 — sign-extend byte (bits[7:0] → bits[31:0])
//   SEXT16 0x061 = 0b01100000001 — sign-extend halfword (bits[15:0] → bits[31:0])
//===----------------------------------------------------------------------===//

// SRA Rd, Ra — Rd = sra(Ra, 1); MSR.C = Ra[0]  (opcode 0x24, func 0x001).
def SRA_   : MBA<0x24, 0x001, (outs GPR:$rD), (ins GPR:$rA),
                 "sra\t$rD, $rA", []> {
  let rB = 0;
}

// SRC Rd, Ra — Rd = (MSR.C << 31) | (Ra >> 1); MSR.C = Ra[0]  (func 0x021).
def SRC_   : MBA<0x24, 0x021, (outs GPR:$rD), (ins GPR:$rA),
                 "src\t$rD, $rA", []> {
  let rB = 0;
}

// SRL Rd, Ra — Rd = Ra >> 1; MSR.C = Ra[0]  (func 0x041).
def SRL_   : MBA<0x24, 0x041, (outs GPR:$rD), (ins GPR:$rA),
                 "srl\t$rD, $rA", []> {
  let rB = 0;
}

// SEXT8 Rd, Ra — Rd = sign_extend_8(Ra)  (func 0x060).
def SEXT8  : MBA<0x24, 0x060, (outs GPR:$rD), (ins GPR:$rA),
                 "sext8\t$rD, $rA",
                 [(set GPR:$rD, (sext_inreg GPR:$rA, i8))]> {
  let rB = 0;
}

// SEXT16 Rd, Ra — Rd = sign_extend_16(Ra)  (func 0x061).
def SEXT16 : MBA<0x24, 0x061, (outs GPR:$rD), (ins GPR:$rA),
                 "sext16\t$rD, $rA",
                 [(set GPR:$rD, (sext_inreg GPR:$rA, i16))]> {
  let rB = 0;
}
