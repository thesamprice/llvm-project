//===-- MicroBlazeISelLowering.h - MicroBlaze DAG Lowering ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEISELLOWERING_H
#define LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEISELLOWERING_H

#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

namespace MicroBlazeISD {
enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,
  RET_FLAG, // Return; operands are chain, glue, and optional return-value regs.
  CALL,     // Direct or indirect call; operand 0 = chain, 1 = callee, rest = args.
  Wrapper,  // Wraps a global/extern symbol for ADDIK-based address materialisation.
  GOT_LOAD, // PIC GOT-indirect load: lwi rD, r20, sym@got (R_MICROBLAZE_GOT_64).
  // Conditional branch after compare-to-zero:
  //   (chain, cond_as_ISD_CondCode_const, diff_reg, dest_bb)
  // diff_reg = LHS - RHS already computed; branch based on sign/zero of diff.
  BR_CC,
  // Conditional select expanded via EmitInstrWithCustomInserter:
  //   (TrueV, FalseV, CC_as_ISD_CondCode_const, diff_reg)
  // Produces TrueV if (diff_reg CC 0), else FalseV.
  SELECT_CC,
  // Signed conditional branch using the CMP instruction (UG984 §5).
  // CMP rD, rA, rB sets bit31=1 iff rA > rB signed; branch opcodes are
  // therefore swapped vs. the subtract path (SETLT→BGTID, SETGT→BLTID,
  // SETLE→BGEID, SETGE→BLEID, SETEQ→BEQID, SETNE→BNEID).
  //   (chain, cond_as_ISD_CondCode_const, LHS, RHS, dest_bb)
  BR_CC_CMP,
  // Signed conditional select using CMP; expanded by EmitInstrWithCustomInserter.
  //   (TrueV, FalseV, CC_as_ISD_CondCode_const, LHS, RHS)
  SELECT_CC_CMP,
  // Unsigned conditional branch using the CMPU instruction (UG984 §5 Fig 84).
  // CMPU sets bit31=1 iff LHS > RHS unsigned; branch opcodes are reversed
  // compared to the CMP path.
  //   (chain, cond_as_ISD_CondCode_const, LHS, RHS, dest_bb)
  BR_CC_CMPU,
  // Unsigned conditional select using CMPU; expanded by EmitInstrWithCustomInserter.
  //   (TrueV, FalseV, CC_as_ISD_CondCode_const, LHS, RHS)
  SELECT_CC_CMPU,
};
} // namespace MicroBlazeISD

class MicroBlazeSubtarget;
class MicroBlazeTargetMachine;

class MicroBlazeTargetLowering : public TargetLowering {
  const MicroBlazeSubtarget &Subtarget;

public:
  explicit MicroBlazeTargetLowering(const MicroBlazeTargetMachine &TM,
                                    const MicroBlazeSubtarget &STI);

  const char *getTargetNodeName(unsigned Opcode) const override;

  // Division is always handled by libcalls (__divsi3, __modsi3, etc.).
  // Returning true prevents DAGCombiner from strength-reducing constant
  // divisions to multiply-high or multiply-lo sequences.
  bool isIntDivCheap(EVT VT, AttributeList Attr) const override {
    return true;
  }

  std::pair<unsigned, const TargetRegisterClass *>
  getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                               StringRef Constraint, MVT VT) const override;

private:
  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerCall(TargetLowering::CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                      SelectionDAG &DAG) const override;

  SDValue LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerExternalSymbol(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBR_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSELECT_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerShift(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVASTART(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVAARG(SDValue Op, SelectionDAG &DAG) const;

  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                               MachineBasicBlock *BB) const override;

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEISELLOWERING_H
