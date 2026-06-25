//===-- MicroBlazeISelDAGToDAG.cpp - MicroBlaze DAG→DAG selector ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeISelDAGToDAG.h"
#include "MicroBlazeISelLowering.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

// Pull in the generated SelectCode body (non-inline mode).
#define GET_DAGISEL_BODY MicroBlazeDAGToDAGISel
#include "MicroBlazeGenDAGISel.inc"
#undef GET_DAGISEL_BODY

//===----------------------------------------------------------------------===//
// Selector constructor
//===----------------------------------------------------------------------===//

MicroBlazeDAGToDAGISel::MicroBlazeDAGToDAGISel(MicroBlazeTargetMachine &TM,
                                               CodeGenOptLevel OptLevel)
    : SelectionDAGISel(TM, OptLevel) {}

//===----------------------------------------------------------------------===//
// Legacy pass wrapper
//===----------------------------------------------------------------------===//

char MicroBlazeDAGToDAGISelLegacy::ID = 0;

MicroBlazeDAGToDAGISelLegacy::MicroBlazeDAGToDAGISelLegacy(
    MicroBlazeTargetMachine &TM, CodeGenOptLevel OptLevel)
    : SelectionDAGISelLegacy(
          ID, std::make_unique<MicroBlazeDAGToDAGISel>(TM, OptLevel)) {}

FunctionPass *llvm::createMicroBlazeISelDag(MicroBlazeTargetMachine &TM,
                                            CodeGenOptLevel OptLevel) {
  return new MicroBlazeDAGToDAGISelLegacy(TM, OptLevel);
}

//===----------------------------------------------------------------------===//
// Address-mode selection
//===----------------------------------------------------------------------===//

// SelectADDRri — match base + signed-16-bit-offset address forms.
bool MicroBlazeDAGToDAGISel::SelectADDRri(SDValue Addr, SDValue &Base,
                                           SDValue &Offset) {
  SDLoc DL(Addr);
  MVT PtrVT = MVT::i32;

  if (auto *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base   = CurDAG->getTargetFrameIndex(FIN->getIndex(), PtrVT);
    Offset = CurDAG->getTargetConstant(0, DL, PtrVT);
    return true;
  }

  if (Addr.getOpcode() == ISD::ADD) {
    if (auto *CN = dyn_cast<ConstantSDNode>(Addr.getOperand(1))) {
      int64_t Imm = CN->getSExtValue();
      if (isInt<16>(Imm)) {
        Base   = Addr.getOperand(0);
        if (auto *FIN = dyn_cast<FrameIndexSDNode>(Base))
          Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), PtrVT);
        Offset = CurDAG->getTargetConstant(Imm, DL, PtrVT);
        return true;
      }
    }
  }

  Base   = Addr;
  Offset = CurDAG->getTargetConstant(0, DL, PtrVT);
  return true;
}

// SelectADDRrr — match base + register-index address forms.
bool MicroBlazeDAGToDAGISel::SelectADDRrr(SDValue Addr, SDValue &Base,
                                           SDValue &Index) {
  if (Addr.getOpcode() == ISD::ADD) {
    Base  = Addr.getOperand(0);
    Index = Addr.getOperand(1);
    return true;
  }
  return false;
}

//===----------------------------------------------------------------------===//
// Select dispatch
//===----------------------------------------------------------------------===//

// Map ISD condition codes to MicroBlaze conditional-branch opcodes.
// MicroBlaze branches test a register against zero: BEQID = 0, BLTID < 0, etc.
// The register operand should be LHS - RHS.
static unsigned getBranchOpcodeForCC(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETEQ:  return MicroBlaze::BEQID;
  case ISD::SETNE:  return MicroBlaze::BNEID;
  case ISD::SETLT:  return MicroBlaze::BLTID;
  case ISD::SETLE:  return MicroBlaze::BLEID;
  case ISD::SETGT:  return MicroBlaze::BGTID;
  case ISD::SETGE:  return MicroBlaze::BGEID;
  // Unsigned: invert operand order for signed arithmetic approximation.
  // For now, fall through to signed variants (TODO: use CMPU).
  case ISD::SETULT: return MicroBlaze::BLTID;
  case ISD::SETULE: return MicroBlaze::BLEID;
  case ISD::SETUGT: return MicroBlaze::BGTID;
  case ISD::SETUGE: return MicroBlaze::BGEID;
  default:
    llvm_unreachable("Unsupported condition code for MicroBlaze branch");
  }
}

void MicroBlazeDAGToDAGISel::Select(SDNode *Node) {
  if (Node->isMachineOpcode()) {
    Node->setNodeId(-1);
    return;
  }

  // Materialize a frame index used as a pointer value (e.g. alloca address
  // passed to a function call): emit ADDIK rd, TargetFrameIndex, 0.
  // eliminateFrameIndex will rewrite this to ADDIK rd, R1, actual_offset.
  if (Node->getOpcode() == ISD::FrameIndex) {
    SDLoc DL(Node);
    int FI = cast<FrameIndexSDNode>(Node)->getIndex();
    SDValue TFI = CurDAG->getTargetFrameIndex(FI, MVT::i32);
    SDValue Zero = CurDAG->getTargetConstant(0, DL, MVT::i32);
    SDNode *N = CurDAG->getMachineNode(MicroBlaze::ADDIK, DL, MVT::i32,
                                        TFI, Zero);
    ReplaceNode(Node, N);
    return;
  }

  // Handle MicroBlazeISD::BR_CC: (chain, cc_const, diff, dest_bb)
  // diff = LHS - RHS (already a SelectionDAG sub node or register).
  if (Node->getOpcode() == MicroBlazeISD::BR_CC) {
    SDLoc DL(Node);
    ISD::CondCode CC = static_cast<ISD::CondCode>(
        cast<ConstantSDNode>(Node->getOperand(1))->getZExtValue());
    SDValue DiffVal = Node->getOperand(2);
    SDValue Dest    = Node->getOperand(3);
    SDValue Chain   = Node->getOperand(0);

    unsigned BrOp = getBranchOpcodeForCC(CC);
    SDNode *Selected = CurDAG->getMachineNode(BrOp, DL, MVT::Other,
                                              {DiffVal, Dest, Chain});
    ReplaceNode(Node, Selected);
    return;
  }

  // Handle MicroBlazeISD::BR_CC_CMPU: (chain, cc_const, LHS, RHS, dest_bb)
  // Emit CMPU rtemp, LHS, RHS then branch using the CMPU-reversed opcode.
  // CMPU sets bit31=1 iff LHS > RHS unsigned (UG984 §5 Fig 84).
  if (Node->getOpcode() == MicroBlazeISD::BR_CC_CMPU) {
    SDLoc DL(Node);
    ISD::CondCode CC = static_cast<ISD::CondCode>(
        cast<ConstantSDNode>(Node->getOperand(1))->getZExtValue());
    SDValue LHS   = Node->getOperand(2);
    SDValue RHS   = Node->getOperand(3);
    SDValue Dest  = Node->getOperand(4);
    SDValue Chain = Node->getOperand(0);

    SDNode *CmpuNode = CurDAG->getMachineNode(MicroBlaze::CMPU, DL, MVT::i32,
                                              {LHS, RHS});
    SDValue CmpuResult(CmpuNode, 0);
    unsigned BrOp;
    switch (CC) {
    case ISD::SETUGT: BrOp = MicroBlaze::BLTID; break;
    case ISD::SETULT: BrOp = MicroBlaze::BGTID; break;
    case ISD::SETUGE: BrOp = MicroBlaze::BLEID; break;
    case ISD::SETULE: BrOp = MicroBlaze::BGEID; break;
    default: llvm_unreachable("Expected unsigned inequality CC in BR_CC_CMPU");
    }
    SDNode *Selected = CurDAG->getMachineNode(BrOp, DL, MVT::Other,
                                              {CmpuResult, Dest, Chain});
    ReplaceNode(Node, Selected);
    return;
  }

  SelectCode(Node);
}
