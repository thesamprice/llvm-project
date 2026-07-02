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

// Width operand for bsefi/bsifi (extract/insert field).
// GAS convention: both instructions take WIDTH as the visible operand.
//   bsefi: hardware bits[10:6] = WIDTH directly (no transform).
//   bsifi: hardware bits[10:6] = shift+width-1; custom encoder does the math.
def bsefi_width : Operand<i32>, ImmLeaf<i32, [{return Imm >= 1 && Imm <= 32;}]> {
  let OperandType = "OPERAND_IMMEDIATE";
}
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
                    "bsrli\t$rD, $rA, $shamt", [], [WriteBarrel]>;
  def BSRAI : MBBSI<0, 1, (outs GPR:$rD), (ins GPR:$rA, uimm5:$shamt),
                    "bsrai\t$rD, $rA, $shamt", [], [WriteBarrel]>;
  def BSLLI : MBBSI<1, 0, (outs GPR:$rD), (ins GPR:$rA, uimm5:$shamt),
                    "bslli\t$rD, $rA, $shamt", [], [WriteBarrel]>;

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
  // GAS / hardware convention (asymmetric between BSEFI and BSIFI):
  //   bsefi rD, rA, WIDTH, START  → bits[10:6] = WIDTH, bits[4:0] = START
  //   bsifi rD, rA, WIDTH, START  → bits[10:6] = START+WIDTH-1, bits[4:0] = START
  def BSEFI : MBBSF<0, 1, (outs GPR:$rD), (ins GPR:$rA, bsefi_width:$immw, uimm5:$imms),
                    "bsefi\t$rD, $rA, $immw, $imms", [], [WriteBarrel]>;
  def BSIFI : MBBSF<1, 0, (outs GPR:$rD),
                    (ins GPR:$rD_src, GPR:$rA, bsifi_width:$immw, uimm5:$imms),
                    "bsifi\t$rD, $rA, $immw, $imms", [], [WriteBarrel]> {
    let Constraints = "$rD_src = $rD";
  }
}
