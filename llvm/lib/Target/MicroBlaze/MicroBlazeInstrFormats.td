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
def HasDividePred : Predicate<"Subtarget->hasDivide()">,
    AssemblerPredicate<(all_of FeatureDivide), "divide">;
def HasReorderInstrPred : Predicate<"Subtarget->hasReorderInstr()">,
    AssemblerPredicate<(all_of FeatureReorderInstr), "reorder">;
def HasHardFloatPred : Predicate<"Subtarget->hasHardFloat()">,
    AssemblerPredicate<(all_of FeatureHardFloat), "hard-float">;
def HasFloatConvertPred : Predicate<"Subtarget->hasFloatConvert()">,
    AssemblerPredicate<(all_of FeatureFloatConvert), "float-convert">;

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
          dag outs, dag ins, string asmstr, list<dag> pattern,
          InstrItinClass itin = IIC_ALU>
    : MBInst<outs, ins, asmstr, pattern, itin> {
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
          dag outs, dag ins, string asmstr, list<dag> pattern,
          InstrItinClass itin = IIC_ALU>
    : MBInst<outs, ins, asmstr, pattern, itin> {
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

//===----------------------------------------------------------------------===//
// BSRLI/BSRAI/BSLLI — barrel-shift immediate (opcode 0x19, UG984 §5 Fig 82)
//   [31:26]=0x19  [25:21]=rD  [20:16]=rA  [15:11]=0  [10]=S  [9]=T
//   [8:5]=0  [4:0]=shamt5
// S=1: shift left; T=1: arithmetic; not a Type B — imm prefix has no effect.
//===----------------------------------------------------------------------===//
class MBBSI<bit S, bit T, dag outs, dag ins, string asmstr, list<dag> pattern,
            InstrItinClass itin = IIC_ALU>
    : MBInst<outs, ins, asmstr, pattern, itin> {
  bits<5> rD;
  bits<5> rA;
  bits<5> shamt;

  let Inst{31-26} = 0x19;
  let Inst{25-21} = rD;
  let Inst{20-16} = rA;
  let Inst{15-11} = 0;
  let Inst{10}    = S;
  let Inst{9}     = T;
  let Inst{8-5}   = 0;
  let Inst{4-0}   = shamt;
}

//===----------------------------------------------------------------------===//
// BSEFI/BSIFI — barrel-shift field extract/insert (opcode 0x19, UG984 §5 Fig 82)
//   [31:26]=0x19  [25:21]=rD  [20:16]=rA  [15]=I  [14]=E  [13:11]=0
//   [10:6]=IMMW  [5]=0  [4:0]=IMMS
// E=1: extract field; I=1: insert field; IMMW=end bit index, IMMS=shift amount.
//===----------------------------------------------------------------------===//
class MBBSF<bit I, bit E, dag outs, dag ins, string asmstr, list<dag> pattern,
            InstrItinClass itin = IIC_ALU>
    : MBInst<outs, ins, asmstr, pattern, itin> {
  bits<5> rD;
  bits<5> rA;
  bits<5> immw;
  bits<5> imms;

  let Inst{31-26} = 0x19;
  let Inst{25-21} = rD;
  let Inst{20-16} = rA;
  let Inst{15}    = I;
  let Inst{14}    = E;
  let Inst{13-11} = 0;
  let Inst{10-6}  = immw;
  let Inst{5}     = 0;
  let Inst{4-0}   = imms;
}

//===----------------------------------------------------------------------===//
// MFS — Move From Special-Purpose Register  (opcode 0x25, UG984 §5 Fig 105)
//   [31:26]=0x25  [25:21]=rD  [20]=0  [19]=E  [18:16]=000  [15]=1  [14]=0
//   [13:0]=rS_addr (14-bit SPR address)
//===----------------------------------------------------------------------===//

class MBMFS<bit E, dag outs, dag ins, string asmstr, list<dag> pattern,
            InstrItinClass itin = IIC_SPR>
    : MBInst<outs, ins, asmstr, pattern, itin> {
  bits<5>  rD;
  bits<14> rs;

  let Inst{31-26} = 0x25;
  let Inst{25-21} = rD;
  let Inst{20}    = 0;
  let Inst{19}    = E;
  let Inst{18-16} = 0;
  let Inst{15}    = 1;
  let Inst{14}    = 0;
  let Inst{13-0}  = rs;
}

//===----------------------------------------------------------------------===//
// MTS — Move To Special-Purpose Register  (opcode 0x25, UG984 §5 Fig 108)
//   [31:26]=0x25  [25]=0  [24]=E  [23:21]=000  [20:16]=rA  [15]=1  [14]=1
//   [13:0]=rS_addr (14-bit SPR address)
//===----------------------------------------------------------------------===//

class MBMTS<bit E, dag outs, dag ins, string asmstr, list<dag> pattern,
            InstrItinClass itin = IIC_SPR>
    : MBInst<outs, ins, asmstr, pattern, itin> {
  bits<5>  rA;
  bits<14> rs;

  let Inst{31-26} = 0x25;
  let Inst{25}    = 0;
  let Inst{24}    = E;
  let Inst{23-21} = 0;
  let Inst{20-16} = rA;
  let Inst{15}    = 1;
  let Inst{14}    = 1;
  let Inst{13-0}  = rs;
}

//===----------------------------------------------------------------------===//
// MSRSET/MSRCLR — Read MSR and set/clear bits  (opcode 0x25, UG984 §5 Figs 106-107)
//   [31:26]=0x25  [25:21]=rD  [20]=1  [19:17]=000  [16]=clr  [15]=0
//   [14:0]=Imm15
//===----------------------------------------------------------------------===//

class MBMSR<bit clr, dag outs, dag ins, string asmstr, list<dag> pattern,
            InstrItinClass itin = IIC_SPR>
    : MBInst<outs, ins, asmstr, pattern, itin> {
  bits<5>  rD;
  bits<15> imm;

  let Inst{31-26} = 0x25;
  let Inst{25-21} = rD;
  let Inst{20}    = 1;
  let Inst{19-17} = 0;
  let Inst{16}    = clr;
  let Inst{15}    = 0;
  let Inst{14-0}  = imm;
}
