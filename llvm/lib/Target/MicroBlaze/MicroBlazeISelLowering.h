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
  // Conditional branch after compare-to-zero:
  //   (chain, cond_as_ISD_CondCode_const, diff_reg, dest_bb)
  // diff_reg = LHS - RHS already computed; branch based on sign/zero of diff.
  BR_CC,
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
  SDValue LowerShift(SDValue Op, SelectionDAG &DAG) const;

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEISELLOWERING_H
