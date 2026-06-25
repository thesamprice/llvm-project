//===-- MicroBlazeInstrALU.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// ALU — register/register (Type A, keep-carry variants: no carry generated)
// Opcode values from UG984 Ch.5 instruction encoding tables.
//===----------------------------------------------------------------------===//

// ADDK Rd, Ra, Rb — add, no carry (opcode=0x04, K=1 C=0). UG984 Fig 59.
def ADDK  : MBA<0x04, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                "addk\t$rD, $rA, $rB", []> {
  let isCommutable = 1;
}

// RSUBK Rd, Ra, Rb — Rd = Rb - Ra, no carry (opcode=0x05). UG984 Ch.5.
// Not commutable: operand order determines which operand is subtracted.
def RSUBK : MBA<0x05, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                "rsubk\t$rD, $rA, $rB", []>;

// AND, OR, XOR — commutable binary operations.
// ANDN (Ra & ~Rb) — not commutable; negation applies to Rb only.
let isCommutable = 1 in {
def AND_  : MBA<0x21, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                "and\t$rD, $rA, $rB", []>;
def OR_   : MBA<0x20, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                "or\t$rD, $rA, $rB", []>;
def XOR_  : MBA<0x22, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                "xor\t$rD, $rA, $rB", []>;
} // let isCommutable = 1

def ANDN  : MBA<0x23, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                "andn\t$rD, $rA, $rB", []>;

// MUL Rd, Ra, Rb (opcode=0x10)
def MUL_  : MBA<0x10, 0, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                "mul\t$rD, $rA, $rB", []> {
  let isCommutable = 1;
}

//===----------------------------------------------------------------------===//
// ALU — register/immediate (Type B)
// Opcode values from UG984 Ch.5 instruction encoding tables.
//===----------------------------------------------------------------------===//

// ADDIK Rd, Ra, imm16 — add immediate, no carry (opcode=0x0C). UG984 Fig 60.
def ADDIK  : MBB<0x0C, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                 "addik\t$rD, $rA, $imm", []>;

// RSUBIK Rd, Ra, imm16 — imm - Ra (opcode=0x0D). UG984 Ch.5.
def RSUBIK : MBB<0x0D, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                 "rsubik\t$rD, $rA, $imm", []>;

// ORI, ANDI, XORI (opcode=0x28, 0x29, 0x2A). UG984 Ch.5 (ori=101000, andi=101001, xori=101010).
def ORI_   : MBB<0x28, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                 "ori\t$rD, $rA, $imm", []>;
def ANDI_  : MBB<0x29, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                 "andi\t$rD, $rA, $imm", []>;
def XORI_  : MBB<0x2A, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                 "xori\t$rD, $rA, $imm", []>;

// MULI Rd, Ra, imm16 (opcode=0x18). UG984 Ch.5 (muli=011000).
def MULI_  : MBB<0x18, (outs GPR:$rD), (ins GPR:$rA, simm16:$imm),
                 "muli\t$rD, $rA, $imm", []>;

//===----------------------------------------------------------------------===//
// Barrel-shift instructions (require +barrel-shift, opcode=0x11)
//===----------------------------------------------------------------------===//
// Function field bits[10:9]: 00=logical-right (bsrl), 01=arith-right (bsra), 10=left (bsll).
// UG984 Ch.5: bsrl func[10:0]=0x000, bsra=0x200, bsll=0x400.

let Predicates = [HasBarrelShiftPred] in {
  def BSRL : MBA<0x11, 0b00000000000, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                 "bsrl\t$rD, $rA, $rB", []>;
  def BSRA : MBA<0x11, 0b01000000000, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                 "bsra\t$rD, $rA, $rB", []>;
  def BSLL : MBA<0x11, 0b10000000000, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                 "bsll\t$rD, $rA, $rB", []>;
}

//===----------------------------------------------------------------------===//
// Multiply-high instructions (require +multiply-high, opcode=0x10)
//===----------------------------------------------------------------------===//

let Predicates = [HasMultiplyHighPred] in {
  // MULH/MULHU are commutable: signed/unsigned high multiply is symmetric.
  // MULHSU (signed×unsigned) is not: operand types differ.
  def MULH   : MBA<0x10, 0b00000000001, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                   "mulh\t$rD, $rA, $rB", []> { let isCommutable = 1; }
  def MULHU  : MBA<0x10, 0b00000000011, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                   "mulhu\t$rD, $rA, $rB", []> { let isCommutable = 1; }
  def MULHSU : MBA<0x10, 0b00000000010, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                   "mulhsu\t$rD, $rA, $rB", []>;
}

//===----------------------------------------------------------------------===//
// Compare instructions (require +pattern-compare, opcode=0x05)
//===----------------------------------------------------------------------===//
// Shared opcode with RSUBK; func[1]=1 selects compare; func[0]=1 unsigned.

// cmp/cmpu share opcode 0x05 with rsubk; func[0]=1 enables compare,
// func[1]=1 selects unsigned. UG984 Ch.5.
// Not commutable: bit31 of result encodes signed(rA > rB), operand order matters.
let Predicates = [HasPatternComparePred] in {
  def CMP  : MBA<0x05, 0b00000000001, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                 "cmp\t$rD, $rA, $rB", []>;
  def CMPU : MBA<0x05, 0b00000000011, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                 "cmpu\t$rD, $rA, $rB", []>;
}

//===----------------------------------------------------------------------===//
// ALU SelectionDAG patterns
//===----------------------------------------------------------------------===//

// ALU reg/reg
def : Pat<(add i32:$ra, i32:$rb), (ADDK  GPR:$ra, GPR:$rb)>;
def : Pat<(sub i32:$rb, i32:$ra), (RSUBK GPR:$ra, GPR:$rb)>;
def : Pat<(and i32:$ra, i32:$rb), (AND_  GPR:$ra, GPR:$rb)>;
def : Pat<(or  i32:$ra, i32:$rb), (OR_   GPR:$ra, GPR:$rb)>;
def : Pat<(xor i32:$ra, i32:$rb), (XOR_  GPR:$ra, GPR:$rb)>;
def : Pat<(mul i32:$ra, i32:$rb), (MUL_  GPR:$ra, GPR:$rb)>;

// ALU reg/simm16
def : Pat<(add i32:$ra, simm16_pat:$imm), (ADDIK  GPR:$ra, imm:$imm)>;
def : Pat<(and i32:$ra, simm16_pat:$imm), (ANDI_  GPR:$ra, imm:$imm)>;
def : Pat<(or  i32:$ra, simm16_pat:$imm), (ORI_   GPR:$ra, imm:$imm)>;
def : Pat<(xor i32:$ra, simm16_pat:$imm), (XORI_  GPR:$ra, imm:$imm)>;
def : Pat<(mul i32:$ra, simm16_pat:$imm), (MULI_  GPR:$ra, imm:$imm)>;

// Small integer constants → ADDIK r0, imm.
def : Pat<(i32 simm16_pat:$imm), (ADDIK R0, imm:$imm)>;

// 32-bit constant materialization.
// LI32 accepts any i32 immediate; it is expanded to ADDIK in MCInstLower.
// The MCCodeEmitter automatically prepends an IMM prefix when the value
// does not fit in a signed 16-bit field (UG984 §5.2).
// isReMaterializable: re-emitting the constant is always cheaper than
// spilling and reloading it from the stack.
def LI32 : MBPseudo<(outs GPR:$rD), (ins i32imm:$imm),
                     "#LI32 $rD, $imm",
                     [(set i32:$rD, imm:$imm)]> {
  let isReMaterializable = 1;
}


// Barrel-shift
let Predicates = [HasBarrelShiftPred] in {
  def : Pat<(srl i32:$ra, i32:$rb), (BSRL GPR:$ra, GPR:$rb)>;
  def : Pat<(sra i32:$ra, i32:$rb), (BSRA GPR:$ra, GPR:$rb)>;
  def : Pat<(shl i32:$ra, i32:$rb), (BSLL GPR:$ra, GPR:$rb)>;
}

// Multiply-high
let Predicates = [HasMultiplyHighPred] in {
  def : Pat<(mulhs i32:$ra, i32:$rb), (MULH  GPR:$ra, GPR:$rb)>;
  def : Pat<(mulhu i32:$ra, i32:$rb), (MULHU GPR:$ra, GPR:$rb)>;
}
