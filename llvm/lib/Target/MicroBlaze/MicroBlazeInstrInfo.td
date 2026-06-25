//===-- MicroBlazeInstrInfo.td - MicroBlaze Instructions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MicroBlaze instruction definitions.  Encoding values from the AMD/Xilinx
// MicroBlaze Processor Reference Guide (UG984).
//
//===----------------------------------------------------------------------===//

include "MicroBlazeInstrFormats.td"

//===----------------------------------------------------------------------===//
// MicroBlaze-specific SelectionDAG nodes
//===----------------------------------------------------------------------===//

def SDT_MBCallSeqStart : SDCallSeqStart<[SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;
def SDT_MBCallSeqEnd   : SDCallSeqEnd<[SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;
def CallSeqStart : SDNode<"ISD::CALLSEQ_START", SDT_MBCallSeqStart,
                          [SDNPHasChain, SDNPOutGlue]>;
def CallSeqEnd   : SDNode<"ISD::CALLSEQ_END",   SDT_MBCallSeqEnd,
                          [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

def SDT_MBCall : SDTypeProfile<0, -1, [SDTCisPtrTy<0>]>;

def MBRetFlag : SDNode<"MicroBlazeISD::RET_FLAG", SDTNone,
    [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

def MBCall : SDNode<"MicroBlazeISD::CALL", SDT_MBCall,
    [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue, SDNPVariadic]>;

def MBWrapper : SDNode<"MicroBlazeISD::Wrapper", SDTIntUnaryOp>;

// MBSelectCC: (TrueV, FalseV, CC_imm, Diff) → result
// Diff = LHS - RHS; CC_imm is an ISD::CondCode integer constant.
// Expanded to a diamond CFG by EmitInstrWithCustomInserter.
def SDT_MBSelectCC : SDTypeProfile<1, 4, [
    SDTCisSameAs<0, 1>, SDTCisSameAs<1, 2>,
    SDTCisVT<3, i32>, SDTCisVT<4, i32>]>;
def MBSelectCC : SDNode<"MicroBlazeISD::SELECT_CC", SDT_MBSelectCC, []>;

//===----------------------------------------------------------------------===//
// Immediate patterns and address-mode complex patterns
//===----------------------------------------------------------------------===//

def simm16_pat : PatLeaf<(imm), [{
    return isInt<16>(N->getSExtValue());
}]>;

// ADDRri: base reg + 16-bit signed offset (covers FrameIndex, add(reg,imm16)).
// The C++ implementation is SelectADDRri in MicroBlazeISelDAGToDAG.cpp.
def ADDRri : ComplexPattern<iPTR, 2, "SelectADDRri", [frameindex, add], []>;

// ADDRrr: base reg + index register.
def ADDRrr : ComplexPattern<iPTR, 2, "SelectADDRrr", [add], []>;

//===----------------------------------------------------------------------===//
// Operand definitions
//===----------------------------------------------------------------------===//

// 16-bit signed immediate.
def simm16 : Operand<i32> {
  let OperandType = "OPERAND_IMMEDIATE";
}

// PC-relative branch target (sign-extended 16-bit offset).
def brtarget : Operand<OtherVT> {
  let OperandType = "OPERAND_PCREL";
  let PrintMethod = "printPCRelImmOperand";
}

// Absolute branch / call target.
def calltarget : Operand<iPTR> {
  let OperandType = "OPERAND_IMMEDIATE";
}

// Memory address: base register + 16-bit signed offset (for ld/st immediate).
def memri : Operand<iPTR> {
  let PrintMethod  = "printMemOperand";
  let MIOperandInfo = (ops GPR:$base, simm16:$offset);
  let EncoderMethod = "getMemOpValue";
  let DecoderMethod = "decodeMemRIOperand";
}

// Memory address: base register + index register (for ld/st register).
def memrr : Operand<iPTR> {
  let PrintMethod  = "printMemOperandRR";
  let MIOperandInfo = (ops GPR:$base, GPR:$index);
  let DecoderMethod = "decodeMemRROperand";
}

//===----------------------------------------------------------------------===//
// Call-frame adjustment pseudo-instructions
//===----------------------------------------------------------------------===//

// ADJCALLSTACKDOWN / ADJCALLSTACKUP lower callseq_start / callseq_end DAG
// nodes. Both are eliminated in MicroBlazeFrameLowering::eliminateCallFramePseudoInstr.
let Defs = [R1], Uses = [R1] in {
  def ADJCALLSTACKDOWN : MBPseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                                  "#ADJCALLSTACKDOWN $amt1 $amt2",
                                  [(CallSeqStart timm:$amt1, timm:$amt2)]>;
  def ADJCALLSTACKUP   : MBPseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                                  "#ADJCALLSTACKUP $amt1 $amt2",
                                  [(CallSeqEnd timm:$amt1, timm:$amt2)]>;
}

//===----------------------------------------------------------------------===//
// NOP pseudo-instruction
//===----------------------------------------------------------------------===//

// NOP encodes as OR r0, r0, r0  (opcode=0x20, all other fields zero).
let hasSideEffects = 0 in
def NOP : MBInst<(outs), (ins), "nop", []> {
  let Inst = 0x80000000; // 0b100000_00000_00000_00000_00000000000
  let AsmString = "nop";
}

//===----------------------------------------------------------------------===//
// IMM prefix instruction
//===----------------------------------------------------------------------===//

// The IMM instruction extends any following Type-B immediate to 32 bits.
// Carries the upper 16 bits; the following instruction carries the lower 16.
// opcode=0x2C  [25:16]=0  [15:0]=imm_hi16
def IMM : MBB<0x2C, (outs), (ins simm16:$imm), "imm\t$imm", []> {
  let rD = 0;
  let rA = 0;
}

include "MicroBlazeInstrALU.td"
include "MicroBlazeInstrMem.td"
include "MicroBlazeInstrBranch.td"
include "MicroBlazeInstrArith.td"
include "MicroBlazeInstrShift.td"
include "MicroBlazeInstrBranch2.td"
include "MicroBlazeInstrReturns.td"
include "MicroBlazeInstrSPR.td"
include "MicroBlazeInstrPCmp.td"
include "MicroBlazeInstrBarrel.td"
include "MicroBlazeInstrDivide.td"
include "MicroBlazeInstrAtomics.td"
include "MicroBlazeInstrReorder.td"
include "MicroBlazeInstrCache.td"
include "MicroBlazeInstrFPU.td"

