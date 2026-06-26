//===-- MicroBlazeISelLowering.cpp - MicroBlaze DAG Lowering --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeISelLowering.h"
#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "MicroBlazeInstrInfo.h"
#include "MicroBlazeMachineFunctionInfo.h"
#include "MicroBlazeSubtarget.h"
#include "MicroBlazeTargetMachine.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RuntimeLibcallUtil.h"
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

  // MicroBlaze is not in LLVM's LegacyDefaultSystemLibrary predicate, so the
  // RuntimeLibcallsInfo table leaves all software-ABI routines as Unsupported.
  // Register them explicitly so that Expand actions emit actual calls.
  setLibcallImpl(RTLIB::SDIV_I32, RTLIB::impl___divsi3);
  setLibcallImpl(RTLIB::UDIV_I32, RTLIB::impl___udivsi3);
  setLibcallImpl(RTLIB::SREM_I32, RTLIB::impl___modsi3);
  setLibcallImpl(RTLIB::UREM_I32, RTLIB::impl___umodsi3);
  setLibcallImpl(RTLIB::MUL_I32,  RTLIB::impl___mulsi3);

  // Memory intrinsics: getMemcpy/getMemset/getMemmove pass the impl enum to
  // getExternalSymbol; if the impl is Unsupported, getLibcallImplName returns
  // a null StringRef which propagates as a null symbol and crashes in LowerCall.
  setLibcallImpl(RTLIB::MEMCPY,  RTLIB::impl_memcpy);
  setLibcallImpl(RTLIB::MEMMOVE, RTLIB::impl_memmove);
  setLibcallImpl(RTLIB::MEMSET,  RTLIB::impl_memset);

  // 64-bit arithmetic: LLVM expands i64 ops on this 32-bit target via libcalls.
  setLibcallImpl(RTLIB::SDIV_I64, RTLIB::impl___divdi3);
  setLibcallImpl(RTLIB::UDIV_I64, RTLIB::impl___udivdi3);
  setLibcallImpl(RTLIB::SREM_I64, RTLIB::impl___moddi3);
  setLibcallImpl(RTLIB::UREM_I64, RTLIB::impl___umoddi3);
  setLibcallImpl(RTLIB::MUL_I64,  RTLIB::impl___muldi3);
  setLibcallImpl(RTLIB::SHL_I64,  RTLIB::impl___ashldi3);
  setLibcallImpl(RTLIB::SRL_I64,  RTLIB::impl___lshrdi3);
  setLibcallImpl(RTLIB::SRA_I64,  RTLIB::impl___ashrdi3);

  // Shifts: legal with barrel-shift (TableGen patterns handle both register and
  // immediate forms). Without barrel-shift, lower to compiler-rt libcalls
  // (__lshlsi3 / __lshrsi3 / __ashrsi3) via Custom lowering; ISD::Expand is
  // intentionally avoided because its ExpandNode path mishandles scalar shifts
  // in release builds (asserts VT.isVector() which is disabled in release).
  if (!STI.hasBarrelShift()) {
    setLibcallImpl(RTLIB::SHL_I32, RTLIB::impl___ashlsi3);
    setLibcallImpl(RTLIB::SRL_I32, RTLIB::impl___lshrsi3);
    setLibcallImpl(RTLIB::SRA_I32, RTLIB::impl___ashrsi3);

    setOperationAction(ISD::SHL, MVT::i32, Custom);
    setOperationAction(ISD::SRL, MVT::i32, Custom);
    setOperationAction(ISD::SRA, MVT::i32, Custom);
  }

  // MUL is always available; high-word variants need +multiply-high.
  if (!STI.hasMultiplyHigh()) {
    setOperationAction(ISD::MULHS,     MVT::i32, Expand);
    setOperationAction(ISD::MULHU,     MVT::i32, Expand);
    // SMUL_LOHI/UMUL_LOHI default to Legal, which causes DAGCombiner to
    // strength-reduce constant divisions into multiply-high sequences that
    // MicroBlaze cannot select.  Mark them Expand so __divsi3/__modsi3
    // libcalls are used for all divisions.
    setOperationAction(ISD::SMUL_LOHI, MVT::i32, Expand);
    setOperationAction(ISD::UMUL_LOHI, MVT::i32, Expand);
  }

  // _Bool is stored as a byte; promote i1 ext-loads to i8 so the existing
  // zextloadi8/extloadi8 patterns can select them.
  setLoadExtAction(ISD::ZEXTLOAD, MVT::i32, MVT::i1,  Promote);
  setLoadExtAction(ISD::EXTLOAD,  MVT::i32, MVT::i1,  Promote);
  setLoadExtAction(ISD::SEXTLOAD, MVT::i32, MVT::i1,  Expand);

  // No sign-extending loads (only zero-extend for bytes/halves).
  setLoadExtAction(ISD::SEXTLOAD, MVT::i32, MVT::i8,  Expand);
  setLoadExtAction(ISD::SEXTLOAD, MVT::i32, MVT::i16, Expand);

  // No native sign-extension instruction. Expand to SHL+SRA pairs; with
  // +barrel-shift those select as bsll/bsra, otherwise they lower to libcalls.
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1,  Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8,  Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Expand);

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
  // SELECT expands to SELECT_CC. SELECT_CC is custom-lowered to
  // MicroBlazeISD::SELECT_CC which is expanded to a diamond CFG by
  // EmitInstrWithCustomInserter. Making SELECT_CC=Custom breaks the
  // SELECT→SELECT_CC→SETCC→SELECT_CC expansion cycle.
  setOperationAction(ISD::SELECT,    MVT::i32, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::i32, Custom);
  setOperationAction(ISD::SETCC,     MVT::i32, Expand);

  // MicroBlaze does not have CTLZ, CTTZ, or CTPOP instructions.
  setOperationAction(ISD::CTLZ,  MVT::i32, Expand);
  setOperationAction(ISD::CTTZ,  MVT::i32, Expand);
  setOperationAction(ISD::CTPOP, MVT::i32, Expand);

  // No jump-table support: fall back to decision trees for all switch stmts.
  setMinimumJumpTableEntries(INT_MAX);

  setMinFunctionAlignment(Align(4));
}

const char *MicroBlazeTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (static_cast<MicroBlazeISD::NodeType>(Opcode)) {
  case MicroBlazeISD::FIRST_NUMBER: break;
  case MicroBlazeISD::RET_FLAG:     return "MicroBlazeISD::RET_FLAG";
  case MicroBlazeISD::CALL:         return "MicroBlazeISD::CALL";
  case MicroBlazeISD::Wrapper:      return "MicroBlazeISD::Wrapper";
  case MicroBlazeISD::BR_CC:         return "MicroBlazeISD::BR_CC";
  case MicroBlazeISD::SELECT_CC:     return "MicroBlazeISD::SELECT_CC";
  case MicroBlazeISD::BR_CC_CMP:     return "MicroBlazeISD::BR_CC_CMP";
  case MicroBlazeISD::SELECT_CC_CMP: return "MicroBlazeISD::SELECT_CC_CMP";
  case MicroBlazeISD::BR_CC_CMPU:    return "MicroBlazeISD::BR_CC_CMPU";
  case MicroBlazeISD::SELECT_CC_CMPU:return "MicroBlazeISD::SELECT_CC_CMPU";
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
  case ISD::SELECT_CC:      return LowerSELECT_CC(Op, DAG);
  case ISD::SHL:
  case ISD::SRL:
  case ISD::SRA:            return LowerShift(Op, DAG);
  default:
    llvm_unreachable("Unexpected custom lowering");
  }
}

SDValue MicroBlazeTargetLowering::LowerSELECT_CC(SDValue Op,
                                                   SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue LHS    = Op.getOperand(0);
  SDValue RHS    = Op.getOperand(1);
  SDValue TrueV  = Op.getOperand(2);
  SDValue FalseV = Op.getOperand(3);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();

  if (Subtarget.hasPatternCompare()) {
    bool IsUnsignedIneq = (CC == ISD::SETUGT || CC == ISD::SETUGE ||
                           CC == ISD::SETULT || CC == ISD::SETULE);
    SDValue CCVal = DAG.getConstant(CC, DL, MVT::i32);
    if (IsUnsignedIneq)
      return DAG.getNode(MicroBlazeISD::SELECT_CC_CMPU, DL, Op.getValueType(),
                         TrueV, FalseV, CCVal, LHS, RHS);
    return DAG.getNode(MicroBlazeISD::SELECT_CC_CMP, DL, Op.getValueType(),
                       TrueV, FalseV, CCVal, LHS, RHS);
  }

  // Fallback: subtract and branch on sign/zero.
  SDValue Diff  = DAG.getNode(ISD::SUB, DL, MVT::i32, LHS, RHS);
  SDValue CCVal = DAG.getConstant(CC, DL, MVT::i32);
  return DAG.getNode(MicroBlazeISD::SELECT_CC, DL, Op.getValueType(),
                     TrueV, FalseV, CCVal, Diff);
}

// Branch opcodes for CMP-based signed comparisons (UG984 §5).
// CMP rD, rA, rB sets bit31=1 iff rA > rB signed; bits30:0 = rB - rA.
// Because bit31=1 means rA>rB, the branch opcodes are swapped vs. the
// subtraction (RSUBK) path where diff=LHS-RHS and LT fires when diff<0:
//   SETLT: want fire when LHS<RHS → bit31=0, bits30:0>0 → result>0 → BGTID
//   SETLE: want fire when LHS≤RHS → result≥0 → BGEID
//   SETGT: want fire when LHS>RHS → bit31=1 → result<0 → BLTID
//   SETGE: want fire when LHS≥RHS → result≤0 → BLEID
static unsigned getMBCmpBranchOpcodeForCC(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETEQ:  return MicroBlaze::BEQID;
  case ISD::SETNE:  return MicroBlaze::BNEID;
  case ISD::SETLT:  return MicroBlaze::BGTID;
  case ISD::SETLE:  return MicroBlaze::BGEID;
  case ISD::SETGT:  return MicroBlaze::BLTID;
  case ISD::SETGE:  return MicroBlaze::BLEID;
  default:
    llvm_unreachable("Expected signed CC for CMP branch");
  }
}

// Branch opcodes for CMPU-based unsigned comparisons (UG984 §5 Fig 84).
// CMPU rD, rA, rB sets bit31=1 iff rA > rB unsigned; bits30:0 = rB - rA.
// Same structural rule as CMP: bit31=1 when rA>rB, so opcodes are swapped
// vs. the subtraction path.
static unsigned getMBCmpuBranchOpcodeForCC(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETUGT: return MicroBlaze::BLTID;
  case ISD::SETULT: return MicroBlaze::BGTID;
  case ISD::SETUGE: return MicroBlaze::BLEID;
  case ISD::SETULE: return MicroBlaze::BGEID;
  default:
    llvm_unreachable("Expected unsigned inequality CC for CMPU branch");
  }
}

// Map ISD::CondCode to the MicroBlaze zero-test branch opcode.
// MicroBlaze conditional branches compare rA to zero; Diff = LHS - RHS is rA.
static unsigned getMBBranchOpcodeForCC(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETEQ:  return MicroBlaze::BEQID;
  case ISD::SETNE:  return MicroBlaze::BNEID;
  case ISD::SETLT:  return MicroBlaze::BLTID;
  case ISD::SETLE:  return MicroBlaze::BLEID;
  case ISD::SETGT:  return MicroBlaze::BGTID;
  case ISD::SETGE:  return MicroBlaze::BGEID;
  // Unsigned: signed branch approximation (correct when values fit in [0,2^31)).
  case ISD::SETULT: return MicroBlaze::BLTID;
  case ISD::SETULE: return MicroBlaze::BLEID;
  case ISD::SETUGT: return MicroBlaze::BGTID;
  case ISD::SETUGE: return MicroBlaze::BGEID;
  default:
    llvm_unreachable("Unsupported condition code for MicroBlaze SELECT_CC");
  }
}

// Shared helper: build the diamond CFG for a conditional select.
// headMBB gets a conditional branch to sinkMBB (true path); fall-through is
// the false path.  BranchReg is the register to test; BrOpc is the opcode.
//
//   headMBB:
//     BrOpc BranchReg, sinkMBB   // condition TRUE → branch to sinkMBB
//   copy0MBB:                    // false-value staging block (empty)
//   sinkMBB:
//     dst = phi [FalseV, copy0MBB], [TrueV, headMBB]
static MachineBasicBlock *
emitSelectCCDiamond(MachineInstr &MI, MachineBasicBlock *BB,
                    unsigned BrOpc, Register BranchReg,
                    Register DstReg, Register TrueReg, Register FalseReg) {
  DebugLoc DL = MI.getDebugLoc();
  MachineFunction *MF = BB->getParent();
  const TargetInstrInfo &TII = *MF->getSubtarget().getInstrInfo();
  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  MachineFunction::iterator It = ++BB->getIterator();

  MachineBasicBlock *headMBB  = BB;
  MachineBasicBlock *copy0MBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *sinkMBB  = MF->CreateMachineBasicBlock(LLVM_BB);
  MF->insert(It, copy0MBB);
  MF->insert(It, sinkMBB);

  sinkMBB->splice(sinkMBB->begin(), headMBB,
                  std::next(MI.getIterator()), headMBB->end());
  sinkMBB->transferSuccessorsAndUpdatePHIs(headMBB);

  BuildMI(headMBB, DL, TII.get(BrOpc)).addReg(BranchReg).addMBB(sinkMBB);
  headMBB->addSuccessor(copy0MBB);
  headMBB->addSuccessor(sinkMBB);
  copy0MBB->addSuccessor(sinkMBB);

  BuildMI(*sinkMBB, sinkMBB->begin(), DL, TII.get(TargetOpcode::PHI), DstReg)
      .addReg(FalseReg).addMBB(copy0MBB)
      .addReg(TrueReg).addMBB(headMBB);

  MI.eraseFromParent();
  return sinkMBB;
}

// Expand SELECT_CC_PSEUDO / SELECT_CC_CMP_PSEUDO / SELECT_CC_CMPU_PSEUDO.
//
// SELECT_CC_PSEUDO operands:      dst(0) TrueV(1) FalseV(2) CC_imm(3) Diff(4)
// SELECT_CC_CMP_PSEUDO operands:  dst(0) TrueV(1) FalseV(2) CC_imm(3) LHS(4) RHS(5)
// SELECT_CC_CMPU_PSEUDO operands: dst(0) TrueV(1) FalseV(2) CC_imm(3) LHS(4) RHS(5)
MachineBasicBlock *MicroBlazeTargetLowering::EmitInstrWithCustomInserter(
    MachineInstr &MI, MachineBasicBlock *BB) const {
  if (MI.getOpcode() == MicroBlaze::SELECT_CC_PSEUDO) {
    ISD::CondCode CC = static_cast<ISD::CondCode>(MI.getOperand(3).getImm());
    Register DiffReg = MI.getOperand(4).getReg();
    return emitSelectCCDiamond(MI, BB, getMBBranchOpcodeForCC(CC), DiffReg,
                               MI.getOperand(0).getReg(),
                               MI.getOperand(1).getReg(),
                               MI.getOperand(2).getReg());
  }

  DebugLoc DL = MI.getDebugLoc();
  MachineFunction *MF = BB->getParent();
  const TargetInstrInfo &TII = *MF->getSubtarget().getInstrInfo();
  ISD::CondCode CC = static_cast<ISD::CondCode>(MI.getOperand(3).getImm());
  Register LHSReg = MI.getOperand(4).getReg();
  Register RHSReg = MI.getOperand(5).getReg();

  if (MI.getOpcode() == MicroBlaze::SELECT_CC_CMP_PSEUDO) {
    // CMP rD, LHS, RHS: bit31 = 1 iff LHS > RHS signed; use CMP-specific opcodes.
    Register CmpReg = MF->getRegInfo().createVirtualRegister(&MicroBlaze::GPRRegClass);
    BuildMI(*BB, MI, DL, TII.get(MicroBlaze::CMP), CmpReg)
        .addReg(LHSReg).addReg(RHSReg);
    return emitSelectCCDiamond(MI, BB, getMBCmpBranchOpcodeForCC(CC), CmpReg,
                               MI.getOperand(0).getReg(),
                               MI.getOperand(1).getReg(),
                               MI.getOperand(2).getReg());
  }

  assert(MI.getOpcode() == MicroBlaze::SELECT_CC_CMPU_PSEUDO &&
         "Unknown custom-inserter pseudo");
  // CMPU rD, LHS, RHS: bit31 = 1 iff LHS > RHS unsigned; branch opcodes reversed.
  Register CmpuReg = MF->getRegInfo().createVirtualRegister(&MicroBlaze::GPRRegClass);
  BuildMI(*BB, MI, DL, TII.get(MicroBlaze::CMPU), CmpuReg)
      .addReg(LHSReg).addReg(RHSReg);
  return emitSelectCCDiamond(MI, BB, getMBCmpuBranchOpcodeForCC(CC), CmpuReg,
                             MI.getOperand(0).getReg(),
                             MI.getOperand(1).getReg(),
                             MI.getOperand(2).getReg());
}

SDValue MicroBlazeTargetLowering::LowerShift(SDValue Op,
                                              SelectionDAG &DAG) const {
  SDLoc DL(Op);
  RTLIB::Libcall LC;
  switch (Op.getOpcode()) {
  case ISD::SHL: LC = RTLIB::getSHL(MVT::i32); break;
  case ISD::SRL: LC = RTLIB::getSRL(MVT::i32); break;
  case ISD::SRA: LC = RTLIB::getSRA(MVT::i32); break;
  default: llvm_unreachable("Unexpected shift opcode");
  }
  SDValue Ops[] = {Op.getOperand(0), Op.getOperand(1)};
  MakeLibCallOptions CallOptions;
  return makeLibCall(DAG, LC, MVT::i32, Ops, CallOptions, DL).first;
}

SDValue MicroBlazeTargetLowering::LowerBR_CC(SDValue Op,
                                               SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS  = Op.getOperand(2);
  SDValue RHS  = Op.getOperand(3);
  SDValue Dest = Op.getOperand(4);

  if (Subtarget.hasPatternCompare()) {
    bool IsUnsignedIneq = (CC == ISD::SETUGT || CC == ISD::SETUGE ||
                           CC == ISD::SETULT || CC == ISD::SETULE);
    SDValue CCVal = DAG.getConstant(CC, DL, MVT::i32);
    if (IsUnsignedIneq)
      // CMPU: bit31=1 iff LHS > RHS unsigned; branch opcodes reversed.
      return DAG.getNode(MicroBlazeISD::BR_CC_CMPU, DL, MVT::Other,
                         Chain, CCVal, LHS, RHS, Dest);
    // CMP: bit31=1 iff LHS < RHS signed (overflow-safe); branch opcodes direct.
    return DAG.getNode(MicroBlazeISD::BR_CC_CMP, DL, MVT::Other,
                       Chain, CCVal, LHS, RHS, Dest);
  }

  // Fallback (no pattern-compare unit): subtract and branch on sign/zero.
  // Note: signed overflow can produce wrong results for SETLT/GT/LE/GE.
  SDValue Diff = DAG.getNode(ISD::SUB, DL, MVT::i32, LHS, RHS);
  SDValue CCVal = DAG.getConstant(CC, DL, MVT::i32);
  return DAG.getNode(MicroBlazeISD::BR_CC, DL, MVT::Other, Chain, CCVal, Diff,
                     Dest);
}

SDValue MicroBlazeTargetLowering::LowerGlobalAddress(SDValue Op,
                                                      SelectionDAG &DAG) const {
  SDLoc DL(Op);
  auto *GAN = cast<GlobalAddressSDNode>(Op);
  SDValue GAWrapper =
      DAG.getTargetGlobalAddress(GAN->getGlobal(), DL, MVT::i32,
                                 GAN->getOffset());
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
  } else {
    // Indirect call: callee is a function pointer or constant address.
    // Copy into a virtual register so BRALD r15, rA can select it.
    Register CalleeReg = MF.getRegInfo().createVirtualRegister(
        &MicroBlaze::GPRRegClass);
    Chain = DAG.getCopyToReg(Chain, DL, CalleeReg, Callee, SDValue());
    Callee = DAG.getRegister(CalleeReg, MVT::i32);
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
