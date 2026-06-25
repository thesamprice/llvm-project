//===-- MicroBlazeISelLowering.cpp - MicroBlaze DAG Lowering --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeISelLowering.h"
#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "MicroBlazeMachineFunctionInfo.h"
#include "MicroBlazeSubtarget.h"
#include "MicroBlazeTargetMachine.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/IR/GlobalValue.h"

using namespace llvm;

// Must follow "using namespace llvm" — generated code uses unqualified names.
#include "MicroBlazeGenCallingConv.inc"

//===----------------------------------------------------------------------===//
// Constructor / configuration
//===----------------------------------------------------------------------===//

MicroBlazeTargetLowering::MicroBlazeTargetLowering(
    const MicroBlazeTargetMachine &TM, const MicroBlazeSubtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI) {
  // i32 is the only native value type.
  addRegisterClass(MVT::i32, &MicroBlaze::GPRRegClass);
  computeRegisterProperties(STI.getRegisterInfo());

  setStackPointerRegisterToSaveRestore(MicroBlaze::R1);
  setBooleanContents(ZeroOrOneBooleanContent);

  // Expand operations that MicroBlaze does not support natively.
  setOperationAction(ISD::SDIV,  MVT::i32, Expand);
  setOperationAction(ISD::UDIV,  MVT::i32, Expand);
  setOperationAction(ISD::SREM,  MVT::i32, Expand);
  setOperationAction(ISD::UREM,  MVT::i32, Expand);
  setOperationAction(ISD::SDIVREM, MVT::i32, Expand);
  setOperationAction(ISD::UDIVREM, MVT::i32, Expand);

  // Shifts: legal only with barrel-shift; otherwise expand to loops.
  if (!STI.hasBarrelShift()) {
    setOperationAction(ISD::SHL, MVT::i32, Expand);
    setOperationAction(ISD::SRL, MVT::i32, Expand);
    setOperationAction(ISD::SRA, MVT::i32, Expand);
  }

  // MUL is always available; high-word variants need +multiply-high.
  if (!STI.hasMultiplyHigh()) {
    setOperationAction(ISD::MULHS, MVT::i32, Expand);
    setOperationAction(ISD::MULHU, MVT::i32, Expand);
  }

  // No sign-extending loads (only zero-extend for bytes/halves).
  setLoadExtAction(ISD::SEXTLOAD, MVT::i32, MVT::i8,  Expand);
  setLoadExtAction(ISD::SEXTLOAD, MVT::i32, MVT::i16, Expand);

  // No floating-point without hard-float feature.
  if (!STI.hasHardFloat()) {
    setOperationAction(ISD::FADD, MVT::f32, Expand);
    setOperationAction(ISD::FSUB, MVT::f32, Expand);
    setOperationAction(ISD::FMUL, MVT::f32, Expand);
    setOperationAction(ISD::FDIV, MVT::f32, Expand);
  }

  // Lower global addresses and external symbols via our wrapper node.
  setOperationAction(ISD::GlobalAddress,  MVT::i32, Custom);
  setOperationAction(ISD::ExternalSymbol, MVT::i32, Custom);

  // Conditional branches: custom-lower BR_CC; BRCOND expands to BR_CC first.
  setOperationAction(ISD::BR_CC,     MVT::i32, Custom);
  setOperationAction(ISD::BRCOND,    MVT::Other, Expand);
  setOperationAction(ISD::SELECT,    MVT::i32, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::i32, Expand);
  setOperationAction(ISD::SETCC,     MVT::i32, Expand);

  // MicroBlaze does not have CTLZ, CTTZ, or CTPOP instructions.
  setOperationAction(ISD::CTLZ,  MVT::i32, Expand);
  setOperationAction(ISD::CTTZ,  MVT::i32, Expand);
  setOperationAction(ISD::CTPOP, MVT::i32, Expand);

  setMinFunctionAlignment(Align(4));
}

const char *MicroBlazeTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (static_cast<MicroBlazeISD::NodeType>(Opcode)) {
  case MicroBlazeISD::FIRST_NUMBER: break;
  case MicroBlazeISD::RET_FLAG:     return "MicroBlazeISD::RET_FLAG";
  case MicroBlazeISD::CALL:         return "MicroBlazeISD::CALL";
  case MicroBlazeISD::Wrapper:      return "MicroBlazeISD::Wrapper";
  case MicroBlazeISD::BR_CC:        return "MicroBlazeISD::BR_CC";
  }
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Custom lowering
//===----------------------------------------------------------------------===//

SDValue MicroBlazeTargetLowering::LowerOperation(SDValue Op,
                                                  SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::GlobalAddress:  return LowerGlobalAddress(Op, DAG);
  case ISD::ExternalSymbol: return LowerExternalSymbol(Op, DAG);
  case ISD::BR_CC:          return LowerBR_CC(Op, DAG);
  default:
    llvm_unreachable("Unexpected custom lowering");
  }
}

SDValue MicroBlazeTargetLowering::LowerBR_CC(SDValue Op,
                                               SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS  = Op.getOperand(2);
  SDValue RHS  = Op.getOperand(3);
  SDValue Dest = Op.getOperand(4);

  // Compute LHS - RHS:  RSUBK(RHS, LHS) = LHS - RHS.
  // We encode this as a sub node; the DAGToDAG selector will emit RSUBK.
  SDValue Diff = DAG.getNode(ISD::SUB, DL, MVT::i32, LHS, RHS);
  SDValue CCVal = DAG.getConstant(CC, DL, MVT::i32);

  return DAG.getNode(MicroBlazeISD::BR_CC, DL, MVT::Other, Chain, CCVal, Diff,
                     Dest);
}

SDValue MicroBlazeTargetLowering::LowerGlobalAddress(SDValue Op,
                                                      SelectionDAG &DAG) const {
  SDLoc DL(Op);
  const GlobalValue *GV = cast<GlobalAddressSDNode>(Op)->getGlobal();
  SDValue GAWrapper = DAG.getTargetGlobalAddress(GV, DL, MVT::i32);
  return DAG.getNode(MicroBlazeISD::Wrapper, DL, MVT::i32, GAWrapper);
}

SDValue
MicroBlazeTargetLowering::LowerExternalSymbol(SDValue Op,
                                               SelectionDAG &DAG) const {
  SDLoc DL(Op);
  const char *Sym = cast<ExternalSymbolSDNode>(Op)->getSymbol();
  SDValue ESWrapper = DAG.getTargetExternalSymbol(Sym, MVT::i32);
  return DAG.getNode(MicroBlazeISD::Wrapper, DL, MVT::i32, ESWrapper);
}

//===----------------------------------------------------------------------===//
// Formal argument lowering
//===----------------------------------------------------------------------===//

SDValue MicroBlazeTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {

  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_MicroBlaze);

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    if (VA.isRegLoc()) {
      const TargetRegisterClass *RC = &MicroBlaze::GPRRegClass;
      Register VReg = MRI.createVirtualRegister(RC);
      MRI.addLiveIn(VA.getLocReg(), VReg);
      SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, VReg, VA.getLocVT());
      InVals.push_back(ArgValue);
    } else {
      assert(VA.isMemLoc() && "Unexpected argument location");
      unsigned ObjSize = VA.getLocVT().getStoreSize();
      int FI = MFI.CreateFixedObject(ObjSize, VA.getLocMemOffset(), true);
      SDValue FIN = DAG.getFrameIndex(FI, MVT::i32);
      InVals.push_back(
          DAG.getLoad(VA.getValVT(), DL, Chain, FIN,
                      MachinePointerInfo::getFixedStack(MF, FI)));
    }
  }

  return Chain;
}

//===----------------------------------------------------------------------===//
// Call lowering
//===----------------------------------------------------------------------===//

SDValue MicroBlazeTargetLowering::LowerCall(
    TargetLowering::CallLoweringInfo &CLI,
    SmallVectorImpl<SDValue> &InVals) const {

  SelectionDAG &DAG     = CLI.DAG;
  SDLoc &DL             = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals     = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins   = CLI.Ins;
  SDValue Chain         = CLI.Chain;
  SDValue Callee        = CLI.Callee;
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg         = CLI.IsVarArg;

  MachineFunction &MF = DAG.getMachineFunction();

  // Analyse outgoing arguments.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_MicroBlaze);

  unsigned StackSize = CCInfo.getStackSize();
  Chain = DAG.getCALLSEQ_START(Chain, StackSize, 0, DL);

  SmallVector<std::pair<unsigned, SDValue>, 4> RegsToPass;
  SmallVector<SDValue, 12> MemOpChains;

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    SDValue Arg = OutVals[i];
    if (VA.isRegLoc()) {
      RegsToPass.push_back({VA.getLocReg(), Arg});
    } else {
      assert(VA.isMemLoc() && "Unexpected argument location");
      SDValue StackPtr = DAG.getRegister(MicroBlaze::R1, MVT::i32);
      SDValue PtrOff =
          DAG.getIntPtrConstant(VA.getLocMemOffset(), DL, /*IsTarget=*/false);
      SDValue MemPtr = DAG.getNode(ISD::ADD, DL, MVT::i32, StackPtr, PtrOff);
      MemOpChains.push_back(DAG.getStore(Chain, DL, Arg, MemPtr,
                                         MachinePointerInfo()));
    }
  }

  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  // Wrap the callee if it is a GlobalAddress or ExternalSymbol.
  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), DL, MVT::i32);
  } else if (ExternalSymbolSDNode *E = dyn_cast<ExternalSymbolSDNode>(Callee)) {
    Callee = DAG.getTargetExternalSymbol(E->getSymbol(), MVT::i32);
  }

  // Build the list of operands and glue.
  SDValue InFlag;
  for (auto &RTP : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, RTP.first, RTP.second, InFlag);
    InFlag = Chain.getValue(1);
  }

  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);
  for (auto &RTP : RegsToPass)
    Ops.push_back(DAG.getRegister(RTP.first, MVT::i32));

  // Add a register mask for the call-clobbered registers.
  const uint32_t *Mask =
      Subtarget.getRegisterInfo()->getCallPreservedMask(MF, CallConv);
  assert(Mask && "Missing call preserved mask for calling convention");
  Ops.push_back(DAG.getRegisterMask(Mask));

  if (InFlag.getNode())
    Ops.push_back(InFlag);

  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  Chain = DAG.getNode(MicroBlazeISD::CALL, DL, NodeTys, Ops);
  InFlag = Chain.getValue(1);

  Chain = DAG.getCALLSEQ_END(Chain, StackSize, 0, InFlag, DL);
  InFlag = Chain.getValue(1);

  // Copy out return values.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState RetCCInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());
  RetCCInfo.AnalyzeCallResult(Ins, RetCC_MicroBlaze);

  for (unsigned i = 0, e = RVLocs.size(); i != e; ++i) {
    auto RVCopy = DAG.getCopyFromReg(Chain, DL, RVLocs[i].getLocReg(),
                                     RVLocs[i].getValVT(), InFlag);
    Chain  = RVCopy.getValue(1);
    InFlag = RVCopy.getValue(2);
    InVals.push_back(RVCopy.getValue(0));
  }

  return Chain;
}

//===----------------------------------------------------------------------===//
// Return lowering
//===----------------------------------------------------------------------===//

SDValue MicroBlazeTargetLowering::LowerReturn(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
    SelectionDAG &DAG) const {

  MachineFunction &MF = DAG.getMachineFunction();

  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_MicroBlaze);

  SDValue Flag;
  SmallVector<SDValue, 4> RetOps(1, Chain);

  for (unsigned i = 0, e = RVLocs.size(); i != e; ++i) {
    CCValAssign &VA = RVLocs[i];
    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), OutVals[i], Flag);
    Flag  = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  RetOps[0] = Chain;
  if (Flag.getNode())
    RetOps.push_back(Flag);

  return DAG.getNode(MicroBlazeISD::RET_FLAG, DL, MVT::Other, RetOps);
}
