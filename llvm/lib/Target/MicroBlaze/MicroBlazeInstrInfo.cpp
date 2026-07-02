//===-- MicroBlazeInstrInfo.cpp - MicroBlaze Instruction Info -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeInstrInfo.h"
#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "MicroBlazeSubtarget.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "MicroBlazeGenInstrInfo.inc"

using namespace llvm;

MicroBlazeInstrInfo::MicroBlazeInstrInfo(const MicroBlazeSubtarget &STI)
    : MicroBlazeGenInstrInfo(STI, *STI.getRegisterInfo(),
                             MicroBlaze::ADJCALLSTACKDOWN,
                             MicroBlaze::ADJCALLSTACKUP),
      RI() {}

void MicroBlazeInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator I,
                                       const DebugLoc &DL, Register DstReg,
                                       Register SrcReg, bool KillSrc,
                                       bool RenamableDst, bool RenamableSrc) const {
  // addk DstReg, SrcReg, R0  (= SrcReg + 0 = SrcReg)
  BuildMI(MBB, I, DL, get(MicroBlaze::ADDK), DstReg)
      .addReg(SrcReg, getKillRegState(KillSrc))
      .addReg(MicroBlaze::R0);
}

void MicroBlazeInstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, Register SrcReg,
    bool IsKill, int FrameIndex, const TargetRegisterClass * /*RC*/,
    Register /*VReg*/, MachineInstr::MIFlag Flags) const {
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();
  BuildMI(MBB, MBBI, DL, get(MicroBlaze::SWI))
      .addReg(SrcReg, getKillRegState(IsKill))
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .setMIFlag(Flags);
}

void MicroBlazeInstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, Register DstReg,
    int FrameIndex, const TargetRegisterClass * /*RC*/, Register /*VReg*/,
    unsigned /*SubReg*/, MachineInstr::MIFlag Flags) const {
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();
  BuildMI(MBB, MBBI, DL, get(MicroBlaze::LWI), DstReg)
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .setMIFlag(Flags);
}

// ---------------------------------------------------------------------------
// Branch analysis helpers (follow SPARC/MIPS pattern)
// ---------------------------------------------------------------------------

static bool isUncondBranchOpcode(unsigned Opc) {
  return Opc == MicroBlaze::BRI || Opc == MicroBlaze::BRID;
}

static bool isCondBranchOpcode(unsigned Opc) {
  switch (Opc) {
  case MicroBlaze::BEQID: case MicroBlaze::BNEID:
  case MicroBlaze::BLTID: case MicroBlaze::BLEID:
  case MicroBlaze::BGTID: case MicroBlaze::BGEID:
  case MicroBlaze::BEQI:  case MicroBlaze::BNEI:
  case MicroBlaze::BLTI:  case MicroBlaze::BLEI:
  case MicroBlaze::BGTI:  case MicroBlaze::BGEI:
    return true;
  default:
    return false;
  }
}

static unsigned getOppositeBranchOpc(unsigned Opc) {
  switch (Opc) {
  case MicroBlaze::BEQID: return MicroBlaze::BNEID;
  case MicroBlaze::BNEID: return MicroBlaze::BEQID;
  case MicroBlaze::BLTID: return MicroBlaze::BGEID;
  case MicroBlaze::BGEID: return MicroBlaze::BLTID;
  case MicroBlaze::BLEID: return MicroBlaze::BGTID;
  case MicroBlaze::BGTID: return MicroBlaze::BLEID;
  case MicroBlaze::BEQI:  return MicroBlaze::BNEI;
  case MicroBlaze::BNEI:  return MicroBlaze::BEQI;
  case MicroBlaze::BLTI:  return MicroBlaze::BGEI;
  case MicroBlaze::BGEI:  return MicroBlaze::BLTI;
  case MicroBlaze::BLEI:  return MicroBlaze::BGTI;
  case MicroBlaze::BGTI:  return MicroBlaze::BLEI;
  default:
    llvm_unreachable("Not a conditional branch opcode");
  }
}

// Cond[0] = opcode-as-imm, Cond[1] = the register operand.
// CBranchID layout: (ins GPR:$rA, brtarget:$imm) → operand(0)=reg, operand(1)=MBB.
static void parseCondBranch(MachineInstr &MI, MachineBasicBlock *&Target,
                             SmallVectorImpl<MachineOperand> &Cond) {
  Target = MI.getOperand(1).getMBB();
  Cond.push_back(MachineOperand::CreateImm(MI.getOpcode()));
  Cond.push_back(MI.getOperand(0));
}

bool MicroBlazeInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                         MachineBasicBlock *&TBB,
                                         MachineBasicBlock *&FBB,
                                         SmallVectorImpl<MachineOperand> &Cond,
                                         bool /*AllowModify*/) const {
  TBB = nullptr;
  FBB = nullptr;
  Cond.clear();

  // Find the last terminator, skipping non-terminators.
  MachineBasicBlock::iterator I = MBB.end();
  if (I == MBB.begin())
    return false;
  --I;
  while (!I->isTerminator()) {
    if (I == MBB.begin())
      return false;
    --I;
  }

  // Cannot analyze bundled instructions (post delay-slot filling).
  if (I->isBundled())
    return true;

  unsigned Opc = I->getOpcode();

  if (isUncondBranchOpcode(Opc)) {
    TBB = I->getOperand(0).getMBB();
    // Check for a preceding conditional branch.
    if (I == MBB.begin())
      return false;
    --I;
    while (!I->isTerminator()) {
      if (I == MBB.begin())
        return false;
      --I;
    }
    if (I->isBundled())
      return true;
    if (isCondBranchOpcode(I->getOpcode())) {
      // Conditional + unconditional: TBB = cond target, FBB = uncond target.
      FBB = TBB;
      parseCondBranch(*I, TBB, Cond);
      return false;
    }
    return true;
  }

  if (isCondBranchOpcode(Opc)) {
    // Conditional branch with fall-through.
    parseCondBranch(*I, TBB, Cond);
    return false;
  }

  return true;
}

unsigned MicroBlazeInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                            int *BytesRemoved) const {
  MachineBasicBlock::iterator I = MBB.end();
  unsigned Count = 0;
  while (I != MBB.begin()) {
    --I;
    if (I->isBundled() || !I->isTerminator())
      break;
    unsigned Opc = I->getOpcode();
    if (!isCondBranchOpcode(Opc) && !isUncondBranchOpcode(Opc))
      break;
    I = MBB.erase(I);
    if (BytesRemoved)
      *BytesRemoved += 4;
    ++Count;
  }
  return Count;
}

bool MicroBlazeInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 2 && "Invalid MicroBlaze branch condition");
  Cond[0].setImm(getOppositeBranchOpc(Cond[0].getImm()));
  return false;
}

unsigned MicroBlazeInstrInfo::insertBranch(MachineBasicBlock &MBB,
                                            MachineBasicBlock *TBB,
                                            MachineBasicBlock *FBB,
                                            ArrayRef<MachineOperand> Cond,
                                            const DebugLoc &DL,
                                            int *BytesAdded) const {
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert((!FBB || Cond.size() == 2) && "Unexpected operands");

  if (Cond.empty()) {
    BuildMI(&MBB, DL, get(MicroBlaze::BRI)).addMBB(TBB);
    if (BytesAdded)
      *BytesAdded = 4;
    return 1;
  }

  // Conditional branch: Cond[0] = D-form opcode, Cond[1] = tested register.
  assert(Cond.size() == 2 && "Unexpected Cond size");
  BuildMI(&MBB, DL, get(Cond[0].getImm())).add(Cond[1]).addMBB(TBB);
  unsigned Count = 1;
  if (BytesAdded)
    *BytesAdded = 4;

  if (FBB) {
    BuildMI(&MBB, DL, get(MicroBlaze::BRI)).addMBB(FBB);
    ++Count;
    if (BytesAdded)
      *BytesAdded += 4;
  }
  return Count;
}
