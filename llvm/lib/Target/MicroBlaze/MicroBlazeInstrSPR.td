//===-- MicroBlazeInstrSPR.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// 14-bit unsigned immediate for SPR address field in MFS/MTS.
def uimm14 : Operand<i32> {
  let OperandType = "OPERAND_IMMEDIATE";
}

// 15-bit unsigned immediate for MSRSET/MSRCLR immediate field.
def uimm15 : Operand<i32> {
  let OperandType = "OPERAND_IMMEDIATE";
}


//===----------------------------------------------------------------------===//
// Special-purpose register move instructions (opcode 0x25, UG984 §5 Figs 105-108)
// All have opcode 0x25; the fixed bits in [20:14] distinguish MFS, MTS, and
// MSRSET/MSRCLR from each other.  Latency: 1 cycle for all four.
//
// SPR address field (14 bits) encodes the special register:
//   0x0000 = PC    0x0001 = MSR   0x0003 = EAR   0x0005 = ESR
//   0x0007 = FSR   0x000B = BTR   0x000D = EDR   0x0800 = SLR
//   0x0802 = SHR   0x1000 = PID   0x1001 = ZPR   0x1002 = TLBX
//   0x1003 = TLBLO 0x1004 = TLBHI 0x2000-0x200C = PVR0-PVR12
//===----------------------------------------------------------------------===//

// MFS rD, rS  — copy special register rS to GPR rD  (E=0).
// Encoding: [19]=0, [15]=1, [14]=0 distinguishes from MTS.
def MFS  : MBMFS<0, (outs GPR:$rD), (ins uimm14:$rs),
                 "mfs\t$rD, $rs", []> {
  let hasSideEffects = 1;
}

// MFSE rD, rS — extended MFS for C_ADDR_SIZE > 32  (E=1; bit[19]=1).
def MFSE : MBMFS<1, (outs GPR:$rD), (ins uimm14:$rs),
                 "mfse\t$rD, $rs", []> {
  let hasSideEffects = 1;
}

// MTS rS, rA  — copy GPR rA to special register rS  (E=0).
// Encoding: [15]=1, [14]=1 distinguishes from MFS.
def MTS  : MBMTS<0, (outs), (ins uimm14:$rs, GPR:$rA),
                 "mts\t$rs, $rA", []> {
  let hasSideEffects = 1;
}

// MTSE rS, rA — extended MTS for C_ADDR_SIZE > 32  (E=1; bit[24]=1).
def MTSE : MBMTS<1, (outs), (ins uimm14:$rs, GPR:$rA),
                 "mtse\t$rs, $rA", []> {
  let hasSideEffects = 1;
}

// MSRSET rD, imm — copy MSR to rD, then set bits in MSR per imm mask.
// Encoding: [20]=1, [16]=0.  Only available when C_USE_MSR_INSTR=1.
def MSRSET : MBMSR<0, (outs GPR:$rD), (ins uimm15:$imm),
                   "msrset\t$rD, $imm", []> {
  let hasSideEffects = 1;
}

// MSRCLR rD, imm — copy MSR to rD, then clear bits in MSR per imm mask.
// Encoding: [20]=1, [16]=1.  Only available when C_USE_MSR_INSTR=1.
def MSRCLR : MBMSR<1, (outs GPR:$rD), (ins uimm15:$imm),
                   "msrclr\t$rD, $imm", []> {
  let hasSideEffects = 1;
}
