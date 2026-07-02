//===-- MicroBlazeInstrBarrel.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// 5-bit unsigned immediate for barrel-shift-immediate shift amount.
// Inheriting ImmLeaf allows this to serve as both an instruction operand
// and a pattern predicate for constant-amount shift matching.
def uimm5 : Operand<i32>, ImmLeaf<i32, [{return Imm >= 0 && Imm < 32;}]> {
  let OperandType = "OPERAND_IMMEDIATE";
}

// Width operand for bsifi (insert field).
// GAS convention: bsifi rD, rA, width, shift → hardware IMMW = shift+width-1.
// The custom encoder translates the user-visible width into the raw IMMW.
def bsifi_width : Operand<i32>, ImmLeaf<i32, [{return Imm >= 1 && Imm <= 32;}]> {
  let OperandType = "OPERAND_IMMEDIATE";
  let EncoderMethod = "getBSIFIImmWValue";
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

}

// Constant-amount shift patterns: lower (srl/sra/shl x, imm5) to BSRLI/BSRAI/BSLLI.
// Register-amount patterns (BSRL/BSRA/BSLL) live in MicroBlazeInstrALU.td.
let Predicates = [HasBarrelShiftPred] in {
  def : Pat<(srl GPR:$ra, uimm5:$shamt), (BSRLI GPR:$ra, uimm5:$shamt)>;
  def : Pat<(sra GPR:$ra, uimm5:$shamt), (BSRAI GPR:$ra, uimm5:$shamt)>;
  def : Pat<(shl GPR:$ra, uimm5:$shamt), (BSLLI GPR:$ra, uimm5:$shamt)>;
}

let Predicates = [HasBarrelShiftPred] in {
  // Bit-field extract/insert (opcode=0x19, E/I bits). UG984 §5 Fig 82.
  // BSEFI: extract field of width (IMMW-IMMS+1) bits starting at bit IMMS.
  // BSIFI: insert field; IMMW = shift + width - 1 per assembler convention.
  def BSEFI : MBBSF<0, 1, (outs GPR:$rD), (ins GPR:$rA, uimm5:$immw, uimm5:$imms),
                    "bsefi\t$rD, $rA, $immw, $imms", []>;
  def BSIFI : MBBSF<1, 0, (outs GPR:$rD), (ins GPR:$rA, bsifi_width:$immw, uimm5:$imms),
                    "bsifi\t$rD, $rA, $immw, $imms", []>;
}
