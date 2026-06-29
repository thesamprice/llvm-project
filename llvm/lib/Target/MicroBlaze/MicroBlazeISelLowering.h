//===-- MicroBlazeISelLowering.h - MicroBlaze DAG Lowering ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEISELLOWERING_H
#define LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEISELLOWERING_H

#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

// Map an f32 ISD::CondCode to FCMP machine opcodes for BR_CC_FP / SELECT_CC_FP.
// Opc1 is always set; Opc2 is nonzero for unordered conditions (OR two results).
// Invert=true means branch/select fires when the FCMP result IS zero (SETO only).
// Returns false if CC is not a valid f32 condition.
bool getFCmpOpcodes(ISD::CondCode CC,
                    unsigned &Opc1, unsigned &Opc2, bool &Invert);

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
  // Float conditional branch using a hardware FCMP instruction (UG984 §5).
  // FCMP_XX rResult, rA, rB writes 1.0 (0x3F800000) if the condition holds,
  // else 0.0; the branch then fires on rResult != 0.
  //   (chain, cond_as_ISD_CondCode_const, LHS_f32, RHS_f32, dest_bb)
  BR_CC_FP,
  // Float conditional select using FCMP; expanded by EmitInstrWithCustomInserter.
  // TrueV/FalseV may be any type (i32 or f32); the comparison is always f32.
  //   (TrueV, FalseV, CC_as_ISD_CondCode_const, LHS_f32, RHS_f32)
  SELECT_CC_FP,
  // Pattern-compare equality/inequality as integer 0/1 (ISD::SETCC value form).
  // PCMPEQ(rA, rB) = 1 if rA == rB, else 0.
  // PCMPNE(rA, rB) = 1 if rA != rB, else 0.
  // Selected by MicroBlazeInstrPCmp.td patterns to PCMPEQ/PCMPNE machine insns.
  PCMPEQ,
  PCMPNE,
  // Carry-chain nodes that thread MSR_C as an explicit i32 DAG value,
  // mirroring ARM's ARMISD::ADDC/ADDE/SUBC/SUBE convention.
  // MSR_C is the physical carry register; it never allocates to a GPR.
  //
  // (result: i32, carry: i32) = ADDC(a, b)   → ADD  machine instruction
  // (result: i32, carry: i32) = ADDE(a, b, carry_in: i32) → ADDC machine instruction
  // (result: i32, carry: i32) = SUBC(a, b)   → RSUB machine instruction
  // (result: i32, carry: i32) = SUBE(a, b, carry_in: i32) → RSUBC machine instruction
  ADDC,
  ADDE,
  SUBC,
  SUBE,
  // Integer absolute value via a branch diamond (expanded by EmitInstrWithCustomInserter).
  // The negation is placed on the fall-through (≤0) path only; src > 0 skips it.
  // Avoids the default SRA+XOR+SUB 3-instruction arithmetic sequence.
  ABS,
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

  bool isIntDivCheap(EVT VT, AttributeList Attr) const override;

  // Returns i32 for all scalar types so carry values stay as i32 through
  // UADDO/UADDO_CARRY chains without needing truncate/zext nodes.
  EVT getSetCCResultType(const DataLayout &DL, LLVMContext &Context,
                         EVT VT) const override;

  SDValue PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const override;

  std::pair<unsigned, const TargetRegisterClass *>
  getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                               StringRef Constraint, MVT VT) const override;

  // Atomic LL/SC expansion via LWX/SWX hardware instructions.
  // AtomicExpandPass calls these at IR level to build the LL/SC retry loop.
  AtomicExpansionKind
  shouldExpandAtomicCmpXchgInIR(const AtomicCmpXchgInst *AI) const override;
  AtomicExpansionKind
  shouldExpandAtomicRMWInIR(const AtomicRMWInst *AI) const override;
  Value *emitLoadLinked(IRBuilderBase &Builder, Type *ValueTy, Value *Addr,
                        AtomicOrdering Ord) const override;
  Value *emitStoreConditional(IRBuilderBase &Builder, Value *Val, Value *Addr,
                              AtomicOrdering Ord) const override;

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
  SDValue LowerConstantPool(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBlockAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerJumpTable(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBR_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSELECT_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSETCC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerATOMIC_FENCE(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerShift(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVASTART(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVAARG(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerFP32Load(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerFP32Store(SDValue Op, SelectionDAG &DAG) const;

  SDValue LowerABS(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerUADDO(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerUSUBO(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerUADDO_CARRY(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerUSUBO_CARRY(SDValue Op, SelectionDAG &DAG) const;

  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                               MachineBasicBlock *BB) const override;

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEISELLOWERING_H
