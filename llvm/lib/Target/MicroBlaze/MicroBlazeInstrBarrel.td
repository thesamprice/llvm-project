//===-- MicroBlazeInstrBarrel.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// 5-bit unsigned immediate for barrel-shift-immediate shift amount.
def uimm5 : Operand<i32> {
  let OperandType = "OPERAND_IMMEDIATE";
}

//===----------------------------------------------------------------------===//
// Barrel-shift immediate and field instructions (require +barrel-shift, opcode 0x19)
// UG984 §5 Fig 82. Not Type B — a preceding imm prefix has no effect on these.
//===----------------------------------------------------------------------===//

let Predicates = [HasBarrelShiftPred] in {
  // Immediate forms (opcode=0x19). Latency: 1 or 2 cycles.
  def BSRLI : MBBSI<0, 0, (outs GPR:$rD), (ins GPR:$rA, uimm5:$shamt),
                    "bsrli\t$rD, $rA, $shamt", []>;
  def BSRAI : MBBSI<0, 1, (outs GPR:$rD), (ins GPR:$rA, uimm5:$shamt),
                    "bsrai\t$rD, $rA, $shamt", []>;
  def BSLLI : MBBSI<1, 0, (outs GPR:$rD), (ins GPR:$rA, uimm5:$shamt),
                    "bslli\t$rD, $rA, $shamt", []>;

  // Bit-field extract/insert (opcode=0x19, E/I bits). UG984 §5 Fig 82.
  // BSEFI: extract field of width (IMMW-IMMS+1) bits starting at bit IMMS.
  // BSIFI: insert field; IMMW = shift + width - 1 per assembler convention.
  def BSEFI : MBBSF<0, 1, (outs GPR:$rD), (ins GPR:$rA, uimm5:$immw, uimm5:$imms),
                    "bsefi\t$rD, $rA, $immw, $imms", []>;
  def BSIFI : MBBSF<1, 0, (outs GPR:$rD), (ins GPR:$rA, uimm5:$immw, uimm5:$imms),
                    "bsifi\t$rD, $rA, $immw, $imms", []>;
}
