//===-- MicroBlazeInstrFPU.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Hardware floating-point instructions (require +hard-float, opcode 0x16)
// UG984 §5 Figs 83-89. C_USE_FPU=1 (Basic) or 2 (Extended). Latency varies.
//   fadd  rD, rA, rB — func=0x000  (4/6/1 cycles area opt 0/1/2)
//   frsub rD, rA, rB — func=0x080  (4/6/1 cycles)
//   fmul  rD, rA, rB — func=0x100  (4/6/1 cycles)
//   fdiv  rD, rA, rB — func=0x180  (30/32/29 cycles)
//   fcmp  rD, rA, rB — func=0x200 | (OpSel << 4); OpSel in bits[6:4]
//     fcmp.un OpSel=0b000 → func=0x200
//     fcmp.lt OpSel=0b001 → func=0x210
//     fcmp.eq OpSel=0b010 → func=0x220
//     fcmp.le OpSel=0b011 → func=0x230
//     fcmp.gt OpSel=0b100 → func=0x240
//     fcmp.ne OpSel=0b101 → func=0x250
//     fcmp.ge OpSel=0b110 → func=0x260
//===----------------------------------------------------------------------===//

let Predicates = [HasHardFloatPred], hasSideEffects = 1 in {
  def FADD  : MBA<0x16, 0x000, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                  "fadd\t$rD, $rA, $rB", [], IIC_FPU>;
  def FRSUB : MBA<0x16, 0x080, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                  "frsub\t$rD, $rA, $rB", [], IIC_FPU>;
  def FMUL  : MBA<0x16, 0x100, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                  "fmul\t$rD, $rA, $rB", [], IIC_FPU>;
  def FDIV  : MBA<0x16, 0x180, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                  "fdiv\t$rD, $rA, $rB", [], IIC_FPU>;

  def FCMP_UN : MBA<0x16, 0x200, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                    "fcmp.un\t$rD, $rA, $rB", [], IIC_FPU>;
  def FCMP_LT : MBA<0x16, 0x210, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                    "fcmp.lt\t$rD, $rA, $rB", [], IIC_FPU>;
  def FCMP_EQ : MBA<0x16, 0x220, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                    "fcmp.eq\t$rD, $rA, $rB", [], IIC_FPU>;
  def FCMP_LE : MBA<0x16, 0x230, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                    "fcmp.le\t$rD, $rA, $rB", [], IIC_FPU>;
  def FCMP_GT : MBA<0x16, 0x240, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                    "fcmp.gt\t$rD, $rA, $rB", [], IIC_FPU>;
  def FCMP_NE : MBA<0x16, 0x250, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                    "fcmp.ne\t$rD, $rA, $rB", [], IIC_FPU>;
  def FCMP_GE : MBA<0x16, 0x260, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                    "fcmp.ge\t$rD, $rA, $rB", [], IIC_FPU>;
}

//===----------------------------------------------------------------------===//
// Floating-point conversion instructions (require +float-convert, opcode 0x16)
// UG984 §5 Figs 90-92. C_USE_FPU=2 (Extended) only.
//   flt   rD, rA — int→float  func=0x280  (5/7/2 cycles area opt 0/1/2)
//   fint  rD, rA — float→int  func=0x300  (4/6/1 cycles)
//   fsqrt rD, rA — sqrt       func=0x380  (27/29/23 cycles)
//===----------------------------------------------------------------------===//

let Predicates = [HasFloatConvertPred], hasSideEffects = 1 in {
  def FLT   : MBA<0x16, 0x280, (outs GPR:$rD), (ins GPR:$rA),
                  "flt\t$rD, $rA", [], IIC_FPU> { let rB = 0; }
  def FINT  : MBA<0x16, 0x300, (outs GPR:$rD), (ins GPR:$rA),
                  "fint\t$rD, $rA", [], IIC_FPU> { let rB = 0; }
  def FSQRT : MBA<0x16, 0x380, (outs GPR:$rD), (ins GPR:$rA),
                  "fsqrt\t$rD, $rA", [], IIC_FPU> { let rB = 0; }
}
