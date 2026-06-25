//===-- MicroBlazeInstrFormats.td - MicroBlaze Instruction Formats -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MicroBlaze instruction formats.
//
// All instructions are 32 bits, big-endian bit numbering (bit 31 = MSB).
//
// Type A — register-register:
//   [31:26] opcode  [25:21] rD  [20:16] rA  [15:11] rB  [10:0] func
//
// Type B — register-immediate:
//   [31:26] opcode  [25:21] rD  [20:16] rA  [15:0] imm16
//
// Large (32-bit) immediates require an IMM prefix instruction that carries
// the high 16 bits:
//   IMM  [31:26]=101100  [25:16]=0  [15:0]=imm_hi16
//   <instr with imm16 = imm_lo16>
//
//===----------------------------------------------------------------------===//

// Subtarget feature predicates
def HasBarrelShiftPred : Predicate<"Subtarget->hasBarrelShift()">,
    AssemblerPredicate<(all_of FeatureBarrelShift), "barrel-shift">;
def HasMultiplyHighPred : Predicate<"Subtarget->hasMultiplyHigh()">,
    AssemblerPredicate<(all_of FeatureMultiplyHigh), "multiply-high">;
def HasPatternComparePred : Predicate<"Subtarget->hasPatternCompare()">,
    AssemblerPredicate<(all_of FeaturePatternCompare), "pattern-compare">;

//===----------------------------------------------------------------------===//
// Base instruction class
//===----------------------------------------------------------------------===//

class MBInst<dag outs, dag ins, string asmstr, list<dag> pattern,
             InstrItinClass itin = NoItinerary>
    : Instruction {
  field bits<32> Inst;
  field bits<32> SoftFail = 0;

  let Namespace    = "MicroBlaze";
  let Size         = 4;
  let OutOperandList = outs;
  let InOperandList  = ins;
  let AsmString      = asmstr;
  let Pattern        = pattern;
  let Itinerary      = itin;

  // Delay-slot annotation (set per-instruction below).
  bit hasDelaySlot = 0;
}

//===----------------------------------------------------------------------===//
// Type A — register/register
//===----------------------------------------------------------------------===//

class MBA<bits<6> op, bits<11> func,
          dag outs, dag ins, string asmstr, list<dag> pattern>
    : MBInst<outs, ins, asmstr, pattern> {
  bits<5> rD;
  bits<5> rA;
  bits<5> rB;

  let Inst{31-26} = op;
  let Inst{25-21} = rD;
  let Inst{20-16} = rA;
  let Inst{15-11} = rB;
  let Inst{10-0}  = func;
}

//===----------------------------------------------------------------------===//
// Type B — register/immediate (imm16 in low 16 bits)
//===----------------------------------------------------------------------===//

class MBB<bits<6> op,
          dag outs, dag ins, string asmstr, list<dag> pattern>
    : MBInst<outs, ins, asmstr, pattern> {
  bits<5>  rD;
  bits<5>  rA;
  bits<16> imm;

  let Inst{31-26} = op;
  let Inst{25-21} = rD;
  let Inst{20-16} = rA;
  let Inst{15-0}  = imm;
}

//===----------------------------------------------------------------------===//
// Pseudo instructions
//===----------------------------------------------------------------------===//

class MBPseudo<dag outs, dag ins, string asmstr, list<dag> pattern>
    : MBInst<outs, ins, asmstr, pattern> {
  let isPseudo = 1;
  let Inst = 0;
}
