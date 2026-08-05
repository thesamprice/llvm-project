//===-- MicroBlazeInstrPCmp.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Pattern-compare instructions (require +pattern-compare)
// UG984 §5 Figs 83, 116-118. C_USE_PCMP_INSTR=1. Latency: 1 cycle each.
//===----------------------------------------------------------------------===//

// DAG nodes for the ISD::SETCC value form (Custom lowered in ISelLowering).
def MBPcmpeq : SDNode<"MicroBlazeISD::PCMPEQ", SDTIntBinOp, [SDNPCommutative]>;
def MBPcmpne : SDNode<"MicroBlazeISD::PCMPNE", SDTIntBinOp, [SDNPCommutative]>;

let Predicates = [HasPatternComparePred] in {
  // pcmpbf/pcmpbc/pcmpeq/pcmpne — pattern compare (UG984 §5 Figs 116-118). Latency: 1 cycle.
  // Share opcodes 0x20-0x23 with or/and/xor/andn; func=0x400 selects pcmp variant.
  // PCMPBF: finds first byte position where a byte of rA matches any byte of rB.
  //         Order-dependent; not commutable.
  // PCMPBC: counts bytes in rA that match any byte in rB (byte-count variant).
  //         Not commutable.
  // PCMPEQ/PCMPNE: test whole-word equality/inequality — commutable.
  def PCMPBF : MBA<0x20, 0x400, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                   "pcmpbf\t$rD, $rA, $rB", []>;
  def PCMPBC : MBA<0x21, 0x400, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                   "pcmpbc\t$rD, $rA, $rB", []>;
  def PCMPEQ : MBA<0x22, 0x400, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                   "pcmpeq\t$rD, $rA, $rB",
                   [(set i32:$rD, (MBPcmpeq i32:$rA, i32:$rB))]> {
    let isCommutable = 1;
  }
  def PCMPNE : MBA<0x23, 0x400, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
                   "pcmpne\t$rD, $rA, $rB",
                   [(set i32:$rD, (MBPcmpne i32:$rA, i32:$rB))]> {
    let isCommutable = 1;
  }

  // clz — count leading zeros (UG984 §5 Fig 83, opcode=0x24, func=0x0E0). Latency: 1 cycle.
  def CLZ    : MBA<0x24, 0x0E0, (outs GPR:$rD), (ins GPR:$rA),
                   "clz\t$rD, $rA", [(set i32:$rD, (ctlz i32:$rA))]> {
    let rB = 0;
  }
}
