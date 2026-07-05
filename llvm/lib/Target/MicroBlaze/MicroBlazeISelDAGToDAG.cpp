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
#include "llvm/IR/IntrinsicsMicroBlaze.h"

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
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), PtrVT);
    Offset = CurDAG->getTargetConstant(0, DL, PtrVT);
    return true;
  }

  if (Addr.getOpcode() == ISD::ADD) {
    SDValue Op0 = Addr.getOperand(0);
    SDValue Op1 = Addr.getOperand(1);
    if (auto *CN = dyn_cast<ConstantSDNode>(Op1)) {
      int64_t Imm = CN->getSExtValue();
      if (isInt<16>(Imm)) {
        Base = Op0;
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

  Base = Addr;
  Offset = CurDAG->getTargetConstant(0, DL, PtrVT);
  return true;
}

// SelectADDRrr — match base + register-index address forms.
bool MicroBlazeDAGToDAGISel::SelectADDRrr(SDValue Addr, SDValue &Base,
                                          SDValue &Index) {
  if (Addr.getOpcode() == ISD::ADD) {
    Base = Addr.getOperand(0);
    Index = Addr.getOperand(1);
    return true;
  }
  return false;
}

// SelectInlineAsmMemoryOperand — handle "m", "o", and "X" inline asm memory
// constraints.  MicroBlaze uses base + signed-16-bit-offset addressing for
// loads and stores, so we decompose via SelectADDRri.  The 'g' ("general")
// constraint is remapped to C_RegisterClass in getConstraintType(), so it
// never reaches here as a memory operand in normal use; this implementation
// covers "m"/"o"/"X" when they appear explicitly in the asm template.
bool MicroBlazeDAGToDAGISel::SelectInlineAsmMemoryOperand(
    const SDValue &Op, InlineAsm::ConstraintCode ConstraintCode,
    std::vector<SDValue> &OutOps) {
  SDValue Base, Offset;
  switch (ConstraintCode) {
  default:
    return true; // unknown constraint — signal failure to the caller
  case InlineAsm::ConstraintCode::m:
  case InlineAsm::ConstraintCode::o:
  case InlineAsm::ConstraintCode::X:
    if (!SelectADDRri(Op, Base, Offset))
      return true;
    break;
  }
  OutOps.push_back(Base);
  OutOps.push_back(Offset);
  return false; // success
}

// SelectFSLImm — match a constant FSL port (0..15) and bind it to rfslN so a
// static FSL get/put can encode the port in the instruction word.
bool MicroBlazeDAGToDAGISel::SelectFSLImm(SDValue N, SDValue &Port) {
  auto *C = dyn_cast<ConstantSDNode>(N);
  if (!C)
    return false;
  uint64_t V = C->getZExtValue();
  if (V > 15)
    return false;
  // rfslN enum values are not guaranteed contiguous (cf. the disassembler's
  // RFSLDecoderTable), so map through an explicit table.
  static const MCPhysReg RFSLRegs[16] = {
      MicroBlaze::rfsl0,  MicroBlaze::rfsl1,  MicroBlaze::rfsl2,
      MicroBlaze::rfsl3,  MicroBlaze::rfsl4,  MicroBlaze::rfsl5,
      MicroBlaze::rfsl6,  MicroBlaze::rfsl7,  MicroBlaze::rfsl8,
      MicroBlaze::rfsl9,  MicroBlaze::rfsl10, MicroBlaze::rfsl11,
      MicroBlaze::rfsl12, MicroBlaze::rfsl13, MicroBlaze::rfsl14,
      MicroBlaze::rfsl15};
  Port = CurDAG->getRegister(RFSLRegs[V], MVT::i32);
  return true;
}

//===----------------------------------------------------------------------===//
// Bit-field extract / insert selectors
//===----------------------------------------------------------------------===//

// Match (and (lshr x, c_shift), c_mask) → BSEFI rD, rA, immw, imms
// GAS / hardware encoding for BSEFI: bits[10:6] = WIDTH, bits[4:0] = START.
// The operand we pass as $immw is therefore the field WIDTH (number of bits),
// not the end-bit index.  (BSIFI is asymmetric: it stores end-bit in the same
// field; its getBSIFIImmWValue encoder converts width→end-bit.)
// Only fire when shift > 0: shift=0 with a small mask is already one ANDI.
bool MicroBlazeDAGToDAGISel::tryBSEFI(SDNode *N) {
  if (!Subtarget->hasBarrelShift())
    return false;
  if (N->getOpcode() != ISD::AND)
    return false;
  auto *MaskC = dyn_cast<ConstantSDNode>(N->getOperand(1));
  if (!MaskC)
    return false;
  uint64_t Mask = MaskC->getZExtValue();
  // Mask must be a non-zero contiguous run of low bits: mask & (mask+1) == 0.
  if (!Mask || (Mask & (Mask + 1)) != 0)
    return false;
  SDValue Src = N->getOperand(0);
  if (Src.getOpcode() != ISD::SRL)
    return false;
  auto *ShiftC = dyn_cast<ConstantSDNode>(Src.getOperand(1));
  if (!ShiftC)
    return false;
  unsigned Shift = ShiftC->getZExtValue();
  if (Shift == 0)
    return false;
  unsigned Width = llvm::popcount(Mask);
  if (Shift + Width > 32)
    return false;
  SDLoc DL(N);
  SDValue Ops[] = {
      Src.getOperand(0),
      CurDAG->getTargetConstant(Width, DL, MVT::i32),
      CurDAG->getTargetConstant(Shift, DL, MVT::i32),
  };
  CurDAG->SelectNodeTo(N, MicroBlaze::BSEFI, MVT::i32, Ops);
  return true;
}

// Match (or (and base, inv_placed_mask) (shl src shift)) → BSIFI rD, rA, width,
// shift BSIFI inserts rA[width-1:0] into rD[shift+width-1:shift], preserving
// other bits. inv_placed_mask must be the bitwise inversion of a contiguous
// shifted field.
bool MicroBlazeDAGToDAGISel::tryBSIFI(SDNode *N) {
  if (!Subtarget->hasBarrelShift())
    return false;
  if (N->getOpcode() != ISD::OR)
    return false;
  // Identify (and base, inv_mask) and the shifted-value operand.
  SDValue AndOp, InsertedOp;
  if (N->getOperand(0).getOpcode() == ISD::AND) {
    AndOp = N->getOperand(0);
    InsertedOp = N->getOperand(1);
  } else if (N->getOperand(1).getOpcode() == ISD::AND) {
    AndOp = N->getOperand(1);
    InsertedOp = N->getOperand(0);
  } else {
    return false;
  }
  auto *InvMaskC = dyn_cast<ConstantSDNode>(AndOp.getOperand(1));
  if (!InvMaskC)
    return false;
  uint64_t PlacedMask = ~InvMaskC->getZExtValue() & 0xFFFFFFFFULL;
  // PlacedMask must be a contiguous shifted field (not zero, not all-ones).
  if (!PlacedMask || PlacedMask == 0xFFFFFFFFULL)
    return false;
  if (!isShiftedMask_32(static_cast<uint32_t>(PlacedMask)))
    return false;
  unsigned Shift = llvm::countr_zero(PlacedMask);
  unsigned Width = llvm::popcount(PlacedMask);
  // The value to insert must be (shl src, Shift).
  if (InsertedOp.getOpcode() != ISD::SHL)
    return false;
  auto *ShlC = dyn_cast<ConstantSDNode>(InsertedOp.getOperand(1));
  if (!ShlC || ShlC->getZExtValue() != Shift)
    return false;
  SDValue BaseVal = AndOp.getOperand(0);
  SDValue SrcVal = InsertedOp.getOperand(0);
  // Operand order matches the tied-register BSIFI definition:
  //   $rD_src = BaseVal (tied to output $rD — the register to insert into)
  //   $rA     = SrcVal  (low Width bits are inserted)
  //   $immw   = Width   (encoder computes IMMW = shift + width - 1)
  //   $imms   = Shift
  SDLoc DL(N);
  SDValue Ops[] = {
      BaseVal,
      SrcVal,
      CurDAG->getTargetConstant(Width, DL, MVT::i32),
      CurDAG->getTargetConstant(Shift, DL, MVT::i32),
  };
  CurDAG->SelectNodeTo(N, MicroBlaze::BSIFI, MVT::i32, Ops);
  return true;
}

//===----------------------------------------------------------------------===//
// Select dispatch
//===----------------------------------------------------------------------===//

// Map ISD condition codes to MicroBlaze conditional-branch opcodes.
// The register operand is LHS - RHS (subtraction-based path, BR_CC).
// BLTID fires when reg < 0, BGTID when reg > 0, etc.
static unsigned getBranchOpcodeForCC(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETEQ:
    return MicroBlaze::BEQID;
  case ISD::SETNE:
    return MicroBlaze::BNEID;
  case ISD::SETLT:
    return MicroBlaze::BLTID;
  case ISD::SETLE:
    return MicroBlaze::BLEID;
  case ISD::SETGT:
    return MicroBlaze::BGTID;
  case ISD::SETGE:
    return MicroBlaze::BGEID;
  case ISD::SETULT:
    return MicroBlaze::BLTID;
  case ISD::SETULE:
    return MicroBlaze::BLEID;
  case ISD::SETUGT:
    return MicroBlaze::BGTID;
  case ISD::SETUGE:
    return MicroBlaze::BGEID;
  default:
    llvm_unreachable("Unsupported condition code for MicroBlaze branch");
  }
}

// Return true if the CMP operands must be swapped (LHS↔RHS) for this CC.
// CMP rD, rA, rB computes rD = rB - rA with rD[31]=1 iff rA > rB (signed).
// Edge case: when (rB - rA) == 0x80000000, the arithmetic bit31=1 but the
// comparison flag wants bit31=0 (rA NOT > rB), so CMP clears it → rD=0.
// For SETLT/SETGE this edge case produces an incorrect result; swapping the
// operands (so rD = rA - rB = LHS - RHS) means the arithmetic bit31 and the
// comparison flag always agree.
// Note: SETEQ/SETNE do not use CMP at all (they use RSUBK), so this only
// needs to return true for the inequality conditions.
static bool cmpNeedsSwap(ISD::CondCode CC) {
  return CC == ISD::SETLT || CC == ISD::SETGE;
}

// Same logic for CMPU: SETULT and SETUGE have the same edge-case issue.
static bool cmpuNeedsSwap(ISD::CondCode CC) {
  return CC == ISD::SETULT || CC == ISD::SETUGE;
}

// Branch opcode to use after the comparison instruction.
// SETEQ/SETNE use RSUBK (not CMP): result = RHS-LHS, zero iff equal.
// SETLT/SETGE use CMP with swapped operands → rD = LHS-RHS, rD[31]=1 iff LHS<RHS.
// SETGT/SETLE use CMP with natural order → rD = RHS-LHS, rD[31]=1 iff LHS>RHS.
static unsigned getCmpBranchOpcodeForCC(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETEQ:
    return MicroBlaze::BEQID;
  case ISD::SETNE:
    return MicroBlaze::BNEID;
  case ISD::SETLT:  // swapped: rD = LHS-RHS, rD[31]=1 iff LHS<RHS → fire when rD<0
    return MicroBlaze::BLTID;
  case ISD::SETLE:  // natural: rD = RHS-LHS, fire when rD≥0 (RHS≥LHS)
    return MicroBlaze::BGEID;
  case ISD::SETGT:  // natural: rD = RHS-LHS, rD[31]=1 iff LHS>RHS → fire when rD<0
    return MicroBlaze::BLTID;
  case ISD::SETGE:  // swapped: rD = LHS-RHS, rD[31]=0 iff LHS≥RHS → fire when rD≥0
    return MicroBlaze::BGEID;
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
    SDNode *N =
        CurDAG->getMachineNode(MicroBlaze::ADDIK, DL, MVT::i32, TFI, Zero);
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
    SDValue Dest = Node->getOperand(3);
    SDValue Chain = Node->getOperand(0);

    unsigned BrOp = getBranchOpcodeForCC(CC);
    SDNode *Selected =
        CurDAG->getMachineNode(BrOp, DL, MVT::Other, {DiffVal, Dest, Chain});
    ReplaceNode(Node, Selected);
    return;
  }

  // Handle MicroBlazeISD::BR_CC_CMP: (chain, cc_const, LHS, RHS, dest_bb)
  // SETEQ/SETNE: use RSUBK (pure subtract, no comparison-bit overwrite).
  //   CMP is wrong for equality because RHS-LHS=0x80000000 gives a false zero
  //   when bit31(comparison) is also 0, e.g. feq(INT_MIN,0) returns T instead of F.
  //   RSUBK(LHS,RHS) = RHS-LHS = 0 iff LHS==RHS for all 32-bit values.
  // SETLT/SETGE: swap CMP operands so rD = LHS-RHS (avoids the same edge case).
  // SETGT/SETLE: natural CMP order (rD = RHS-LHS).
  if (Node->getOpcode() == MicroBlazeISD::BR_CC_CMP) {
    SDLoc DL(Node);
    ISD::CondCode CC = static_cast<ISD::CondCode>(
        cast<ConstantSDNode>(Node->getOperand(1))->getZExtValue());
    SDValue LHS = Node->getOperand(2);
    SDValue RHS = Node->getOperand(3);
    SDValue Dest = Node->getOperand(4);
    SDValue Chain = Node->getOperand(0);
    bool UseRSub = (CC == ISD::SETEQ || CC == ISD::SETNE);
    bool Swap = !UseRSub && cmpNeedsSwap(CC);
    unsigned CmpOpc = UseRSub ? MicroBlaze::RSUBK : MicroBlaze::CMP;
    SDNode *CmpNode = CurDAG->getMachineNode(CmpOpc, DL, MVT::i32,
                                              {Swap ? RHS : LHS,
                                               Swap ? LHS : RHS});
    SDValue CmpResult(CmpNode, 0);
    unsigned BrOp = getCmpBranchOpcodeForCC(CC);
    SDNode *Selected =
        CurDAG->getMachineNode(BrOp, DL, MVT::Other, {CmpResult, Dest, Chain});
    ReplaceNode(Node, Selected);
    return;
  }

  // Handle MicroBlazeISD::BR_CC_CMPU: (chain, cc_const, LHS, RHS, dest_bb)
  // CMPU rD, rA, rB: rD = rB-rA, rD[31]=1 iff rA > rB (unsigned).
  // SETULT/SETUGE: swap operands so rD = LHS-RHS (bit31=1 iff LHS<RHS unsigned),
  // avoiding the same edge case as CMP with 0x80000000.
  if (Node->getOpcode() == MicroBlazeISD::BR_CC_CMPU) {
    SDLoc DL(Node);
    ISD::CondCode CC = static_cast<ISD::CondCode>(
        cast<ConstantSDNode>(Node->getOperand(1))->getZExtValue());
    SDValue LHS = Node->getOperand(2);
    SDValue RHS = Node->getOperand(3);
    SDValue Dest = Node->getOperand(4);
    SDValue Chain = Node->getOperand(0);

    bool Swap = cmpuNeedsSwap(CC);
    SDNode *CmpuNode = CurDAG->getMachineNode(MicroBlaze::CMPU, DL, MVT::i32,
                                               {Swap ? RHS : LHS,
                                                Swap ? LHS : RHS});
    SDValue CmpuResult(CmpuNode, 0);
    unsigned BrOp;
    switch (CC) {
    case ISD::SETUGT:
      BrOp = MicroBlaze::BLTID;  // natural: rD[31]=1 iff LHS>RHS → rD<0
      break;
    case ISD::SETULT:
      BrOp = MicroBlaze::BLTID;  // swapped: rD = LHS-RHS, rD[31]=1 iff LHS<RHS
      break;
    case ISD::SETUGE:
      BrOp = MicroBlaze::BGEID;  // swapped: rD = LHS-RHS, rD[31]=0 iff LHS≥RHS
      break;
    case ISD::SETULE:
      BrOp = MicroBlaze::BGEID;  // natural: rD = RHS-LHS, rD≥0 iff LHS≤RHS
      break;
    default:
      llvm_unreachable("Expected unsigned inequality CC in BR_CC_CMPU");
    }
    SDNode *Selected =
        CurDAG->getMachineNode(BrOp, DL, MVT::Other, {CmpuResult, Dest, Chain});
    ReplaceNode(Node, Selected);
    return;
  }

  // Handle MicroBlazeISD::BR_CC_FP: (chain, cc_const, LHS_f32, RHS_f32,
  // dest_bb) FCMP writes 0x3F800000 (1.0) if condition holds, else 0x00000000
  // (0.0). Treating that value as an integer: 0x3F800000 != 0, so BNEID fires
  // on true. BEQID fires on false — used for the SETO (ordered) condition
  // (inverted). For unordered conditions we OR two FCMP results before
  // branching (UG984 §5).
  if (Node->getOpcode() == MicroBlazeISD::BR_CC_FP) {
    SDLoc DL(Node);
    ISD::CondCode CC = static_cast<ISD::CondCode>(
        cast<ConstantSDNode>(Node->getOperand(1))->getZExtValue());
    SDValue LHS = Node->getOperand(2);
    SDValue RHS = Node->getOperand(3);
    SDValue Dest = Node->getOperand(4);
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
    SDNode *Selected =
        CurDAG->getMachineNode(BrOpc, DL, MVT::Other, {FlagVal, Dest, Chain});
    ReplaceNode(Node, Selected);
    return;
  }

  // Handle MicroBlazeISD::SELECT_CC_FP: (TrueV, FalseV, CC_const, LHS_f32,
  // RHS_f32) Choose the pseudo based on the result type:
  // i32→SELECT_CC_FP_PSEUDO, f32→SELECT_CC_FP_F32_PSEUDO.
  // EmitInstrWithCustomInserter expands both to an FCMP (plus optional OR for
  // unordered conditions) + diamond CFG.
  if (Node->getOpcode() == MicroBlazeISD::SELECT_CC_FP) {
    SDLoc DL(Node);
    SDValue TrueV = Node->getOperand(0);
    SDValue FalseV = Node->getOperand(1);
    SDValue CCConst = Node->getOperand(2);
    SDValue LHS = Node->getOperand(3);
    SDValue RHS = Node->getOperand(4);
    MVT ResultVT = Node->getSimpleValueType(0);

    SDValue TargetCC = CurDAG->getTargetConstant(
        cast<ConstantSDNode>(CCConst)->getZExtValue(), DL, MVT::i32);
    unsigned PseudoOpc = (ResultVT == MVT::f32)
                             ? MicroBlaze::SELECT_CC_FP_F32_PSEUDO
                             : MicroBlaze::SELECT_CC_FP_PSEUDO;
    SDNode *Selected = CurDAG->getMachineNode(
        PseudoOpc, DL, ResultVT, {TrueV, FalseV, TargetCC, LHS, RHS});
    ReplaceNode(Node, Selected);
    return;
  }

  if (Node->getOpcode() == ISD::AND && tryBSEFI(Node))
    return;
  if (Node->getOpcode() == ISD::OR && tryBSIFI(Node))
    return;

  SelectCode(Node);
}
