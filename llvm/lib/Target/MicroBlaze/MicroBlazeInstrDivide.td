//===-- MicroBlazeInstrDivide.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Integer divide (require +divide, opcode=0x12)
// UG984 §5 Fig 95: U bit at std bit[1] → func=0x000 (signed) or 0x002 (unsigned).
// Latency: 1 cycle if rA=0; otherwise 34/35/30 cycles per C_AREA_OPTIMIZED.
//===----------------------------------------------------------------------===//

let Predicates = [HasDividePred] in {
  def IDIV  : MBA<0x12, 0x000, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                  "idiv\t$rD, $rA, $rB", [], IIC_DIV> { let hasSideEffects = 1; }
  def IDIVU : MBA<0x12, 0x002, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                  "idivu\t$rD, $rA, $rB", [], IIC_DIV> { let hasSideEffects = 1; }
}

