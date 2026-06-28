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
    SDValue Op0 = Addr.getOperand(0);
    SDValue Op1 = Addr.getOperand(1);
    if (auto *CN = dyn_cast<ConstantSDNode>(Op1)) {
      int64_t Imm = CN->getSExtValue();
      if (isInt<16>(Imm)) {
        Base   = Op0;
        if (auto *FIN = dyn_cast<FrameIndexSDNode>(Base))
          Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), PtrVT);
        Offset = CurDAG->getTargetConstant(Imm, DL, PtrVT);
        return true;
      }
    }
    // Non-const or out-of-range ADD operand: yield to ADDRrr → lw rd, ra, rb.
    if (!isa<FrameIndexSDNode>(Op0) && !isa<FrameIndexSDNode>(Op1))
      return false;
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
// The register operand is LHS - RHS (subtraction-based path, BR_CC).
// BLTID fires when reg < 0, BGTID when reg > 0, etc.
static unsigned getBranchOpcodeForCC(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETEQ:  return MicroBlaze::BEQID;
  case ISD::SETNE:  return MicroBlaze::BNEID;
  case ISD::SETLT:  return MicroBlaze::BLTID;
  case ISD::SETLE:  return MicroBlaze::BLEID;
  case ISD::SETGT:  return MicroBlaze::BGTID;
  case ISD::SETGE:  return MicroBlaze::BGEID;
  case ISD::SETULT: return MicroBlaze::BLTID;
  case ISD::SETULE: return MicroBlaze::BLEID;
  case ISD::SETUGT: return MicroBlaze::BGTID;
  case ISD::SETUGE: return MicroBlaze::BGEID;
  default:
    llvm_unreachable("Unsupported condition code for MicroBlaze branch");
  }
}

// Map signed ISD condition codes to MicroBlaze branch opcodes for the CMP path.
// CMP rD, rA, rB sets bit31=1 iff rA > rB signed; bits30:0 = rB - rA.
// So rD > 0 when rA < rB, rD < 0 when rA > rB, rD == 0 when rA == rB.
static unsigned getCmpBranchOpcodeForCC(ISD::CondCode CC) {
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

  // Handle MicroBlazeISD::BR_CC_CMP: (chain, cc_const, LHS, RHS, dest_bb)
  // CMP rD, LHS, RHS: bit31=1 iff LHS > RHS signed (overflow-safe).
  // Use getCmpBranchOpcodeForCC (LT↔GT, LE↔GE swapped vs subtraction path).
  if (Node->getOpcode() == MicroBlazeISD::BR_CC_CMP) {
    SDLoc DL(Node);
    ISD::CondCode CC = static_cast<ISD::CondCode>(
        cast<ConstantSDNode>(Node->getOperand(1))->getZExtValue());
    SDValue LHS   = Node->getOperand(2);
    SDValue RHS   = Node->getOperand(3);
    SDValue Dest  = Node->getOperand(4);
    SDValue Chain = Node->getOperand(0);
    SDNode *CmpNode = CurDAG->getMachineNode(MicroBlaze::CMP, DL, MVT::i32,
                                             {LHS, RHS});
    SDValue CmpResult(CmpNode, 0);
    unsigned BrOp = getCmpBranchOpcodeForCC(CC);
    SDNode *Selected = CurDAG->getMachineNode(BrOp, DL, MVT::Other,
                                             {CmpResult, Dest, Chain});
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

  // Handle MicroBlazeISD::BR_CC_FP: (chain, cc_const, LHS_f32, RHS_f32, dest_bb)
  // FCMP writes 0x3F800000 (1.0) if condition holds, else 0x00000000 (0.0).
  // Treating that value as an integer: 0x3F800000 != 0, so BNEID fires on true.
  // BEQID fires on false — used for the SETO (ordered) condition (inverted).
  // For unordered conditions we OR two FCMP results before branching (UG984 §5).
  if (Node->getOpcode() == MicroBlazeISD::BR_CC_FP) {
    SDLoc DL(Node);
    ISD::CondCode CC = static_cast<ISD::CondCode>(
        cast<ConstantSDNode>(Node->getOperand(1))->getZExtValue());
    SDValue LHS   = Node->getOperand(2);
    SDValue RHS   = Node->getOperand(3);
    SDValue Dest  = Node->getOperand(4);
    SDValue Chain = Node->getOperand(0);

    unsigned Opc1, Opc2;
    bool Invert;
    if (!getFCmpOpcodes(CC, Opc1, Opc2, Invert))
      llvm_unreachable("Unhandled f32 condition code in BR_CC_FP");

    SDNode *Cmp1Node = CurDAG->getMachineNode(Opc1, DL, MVT::i32, {LHS, RHS});
    SDValue FlagVal(Cmp1Node, 0);

    if (Opc2) {
      // Unordered variant: OR the two FCMP results; fire on nonzero.
      SDNode *Cmp2Node = CurDAG->getMachineNode(Opc2, DL, MVT::i32, {LHS, RHS});
      SDValue Flag2(Cmp2Node, 0);
      SDNode *OrNode = CurDAG->getMachineNode(MicroBlaze::OR_, DL, MVT::i32,
                                              {FlagVal, Flag2});
      FlagVal = SDValue(OrNode, 0);
    }

    unsigned BrOpc = Invert ? MicroBlaze::BEQID : MicroBlaze::BNEID;
    SDNode *Selected = CurDAG->getMachineNode(BrOpc, DL, MVT::Other,
                                              {FlagVal, Dest, Chain});
    ReplaceNode(Node, Selected);
    return;
  }

  // Handle MicroBlazeISD::SELECT_CC_FP: (TrueV, FalseV, CC_const, LHS_f32, RHS_f32)
  // Choose the pseudo based on the result type: i32→SELECT_CC_FP_PSEUDO,
  // f32→SELECT_CC_FP_F32_PSEUDO.  EmitInstrWithCustomInserter expands both
  // to an FCMP (plus optional OR for unordered conditions) + diamond CFG.
  if (Node->getOpcode() == MicroBlazeISD::SELECT_CC_FP) {
    SDLoc DL(Node);
    SDValue TrueV  = Node->getOperand(0);
    SDValue FalseV = Node->getOperand(1);
    SDValue CCConst = Node->getOperand(2);
    SDValue LHS    = Node->getOperand(3);
    SDValue RHS    = Node->getOperand(4);
    MVT ResultVT   = Node->getSimpleValueType(0);

    SDValue TargetCC = CurDAG->getTargetConstant(
        cast<ConstantSDNode>(CCConst)->getZExtValue(), DL, MVT::i32);
    unsigned PseudoOpc = (ResultVT == MVT::f32) ? MicroBlaze::SELECT_CC_FP_F32_PSEUDO
                                                 : MicroBlaze::SELECT_CC_FP_PSEUDO;
    SDNode *Selected = CurDAG->getMachineNode(PseudoOpc, DL, ResultVT,
                                              {TrueV, FalseV, TargetCC, LHS, RHS});
    ReplaceNode(Node, Selected);
    return;
  }

  SelectCode(Node);
}
