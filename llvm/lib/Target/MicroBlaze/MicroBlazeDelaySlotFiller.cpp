//===-- MicroBlazeDelaySlotFiller.cpp - Fill MicroBlaze delay slots -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MicroBlaze branches and returns have a one-instruction delay slot (UG984 §5).
// The delay slot executes unconditionally after the branch instruction but
// before the branch target is entered.  Importantly, the branch evaluates its
// condition register *before* the delay slot runs, so the delay slot may freely
// write registers that the branch reads — EXCEPT for loop backedges where the
// same branch fires again on the next iteration.  To be safe we never hoist an
// instruction that defines a register the branch reads (conservative rule that
// avoids first-iteration undefined-register bugs on backedges).
//
// The filler uses two strategies, tried in order:
//
//   1. Backward search — scan backward within the current basic block for an
//      instruction that is safe to hoist.  If found, splice it into the slot.
//
//   2. Successor BB search — if backward search fails for a terminator, pick
//      the hottest successor block and try to hoist its first instruction into
//      all predecessor delay slots simultaneously, cloning as needed.  This
//      mirrors the approach used by the MIPS delay slot filler.
//
// If neither strategy succeeds:
//
//   - For conditional/unconditional branches: switch to the equivalent no-delay
//     opcode (e.g. BNEID → BNEI).  Semantics are identical; the no-delay form
//     just does not require a following instruction.
//
//   - For calls and returns (BRALID, BRALD, BRLID, BRLD, RTSD, RTID, RTBD,
//     RTED): the MicroBlaze ISA provides no no-delay equivalent.  A NOP is
//     inserted into the delay slot.
//
//===----------------------------------------------------------------------===//

#include "MicroBlaze.h"
#include "MicroBlazeInstrInfo.h"
#include "MicroBlazeSubtarget.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineBranchProbabilityInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_INSTRINFO_ENUM
#include "MicroBlazeGenInstrInfo.inc"

using namespace llvm;

#define DEBUG_TYPE "microblaze-delay-slot-filler"

STATISTIC(FilledSlots, "Number of delay slots filled");
STATISTIC(UsefulSlots, "Number of delay slots filled with non-NOP instructions");

namespace {

// Maps each predecessor block to the branch instruction whose delay slot will
// receive the cloned filler, or nullptr if the predecessor falls straight
// through (no branch) and the clone should be appended to its end.
using BB2BrMap = SmallDenseMap<MachineBasicBlock *, MachineInstr *, 2>;

class MicroBlazeDelaySlotFiller : public MachineFunctionPass {
  // Branches whose delay slots have already been resolved during this pass.
  // Used by examinePred to detect predecessors that can no longer accept a
  // clone (their slot was filled, NOPed, or converted to a no-delay opcode).
  mutable SmallPtrSet<const MachineInstr *, 8> FilledBranches;

public:
  static char ID;

  MicroBlazeDelaySlotFiller() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "MicroBlaze delay slot filler";
  }

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().setNoVRegs();
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineBranchProbabilityInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  /// Search backward within MBB from BranchIt for an instruction safe to hoist
  /// into the delay slot.  On success, splices it in and returns true.
  bool searchBackward(MachineBasicBlock &MBB,
                      MachineBasicBlock::iterator BranchIt,
                      const TargetInstrInfo *TII) const;

  /// Search the hottest successor of MBB for an instruction that can be cloned
  /// into the delay slots of all predecessors of that successor.  Mirrors
  /// MipsDelaySlotFiller::searchSuccBBs.
  bool searchSuccBBs(MachineBasicBlock &MBB,
                     MachineBasicBlock::iterator Slot,
                     const TargetInstrInfo *TII) const;

  /// Pick the highest-probability non-EH-pad successor of MBB, or nullptr.
  MachineBasicBlock *selectSuccBB(MachineBasicBlock &MBB) const;

  /// Examine predecessor Pred of Succ.  Records whether Pred has an unoccupied
  /// delay slot that branches to Succ (or falls through), accumulates banned
  /// register defs from other successor live-ins, and adds to BrMap.
  /// Returns false if Pred cannot accept a clone.
  bool examinePred(MachineBasicBlock &Pred, const MachineBasicBlock &Succ,
                   SmallSet<MCPhysReg, 8> &BannedDefs, bool &HasMultipleSuccs,
                   BB2BrMap &BrMap) const;
};

} // namespace

char MicroBlazeDelaySlotFiller::ID = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Return the no-delay equivalent opcode for a delayed branch, or 0 if none
// exists.  Calls (BRALID, BRALD, BRLID, BRLD) and returns (RTSD, RTID, RTBD,
// RTED) have no no-delay form in the MicroBlaze ISA; they always need a slot.
static unsigned getNoDelayVariant(unsigned Opc) {
  switch (Opc) {
  // Unconditional branches — immediate target
  case MicroBlaze::BRID:  return MicroBlaze::BRI;
  case MicroBlaze::BRAID: return MicroBlaze::BRAI;
  // Unconditional branches — register target
  case MicroBlaze::BRD:   return MicroBlaze::BR_;
  case MicroBlaze::BRAD:  return MicroBlaze::BRA;
  // Conditional branches — immediate target
  case MicroBlaze::BEQID: return MicroBlaze::BEQI;
  case MicroBlaze::BNEID: return MicroBlaze::BNEI;
  case MicroBlaze::BLTID: return MicroBlaze::BLTI;
  case MicroBlaze::BLEID: return MicroBlaze::BLEI;
  case MicroBlaze::BGTID: return MicroBlaze::BGTI;
  case MicroBlaze::BGEID: return MicroBlaze::BGEI;
  // Conditional branches — register target
  case MicroBlaze::BEQD:  return MicroBlaze::BEQ;
  case MicroBlaze::BNED:  return MicroBlaze::BNE;
  case MicroBlaze::BLTD:  return MicroBlaze::BLT;
  case MicroBlaze::BLED:  return MicroBlaze::BLE;
  case MicroBlaze::BGTD:  return MicroBlaze::BGT;
  case MicroBlaze::BGED:  return MicroBlaze::BGE;
  default: return 0;
  }
}

// Collect all register numbers explicitly read/written by MI into the sets.
// MicroBlaze has no sub-registers, so MCPhysReg identity is sufficient.
static void collectRegs(const MachineInstr &MI,
                        SmallSet<MCPhysReg, 8> &Defs,
                        SmallSet<MCPhysReg, 8> &Uses) {
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isReg() || !MO.getReg().isPhysical())
      continue;
    MCPhysReg R = MO.getReg().asMCReg();
    if (MO.isDef())
      Defs.insert(R);
    else
      Uses.insert(R);
  }
}

static bool overlaps(const SmallSet<MCPhysReg, 8> &A,
                     const SmallSet<MCPhysReg, 8> &B) {
  for (MCPhysReg R : A)
    if (B.count(R))
      return true;
  return false;
}

// Insert clones of Filler into each predecessor's delay slot (or block end).
// Mirrors MipsDelaySlotFiller::insertDelayFiller.
static void insertDelayFiller(MachineBasicBlock::iterator Filler,
                               const BB2BrMap &BrMap) {
  MachineFunction *MF = Filler->getParent()->getParent();
  for (const auto &[Pred, BrMI] : BrMap) {
    MachineInstr *Clone = MF->CloneMachineInstr(&*Filler);
    if (BrMI) {
      Pred->insert(std::next(MachineBasicBlock::iterator(BrMI)), Clone);
    } else {
      Pred->push_back(Clone);
    }
    ++UsefulSlots;
  }
}

// Register any defs of Filler as live-in to MBB (needed after hoisting out).
// Mirrors MipsDelaySlotFiller::addLiveInRegs.
static void addLiveInRegs(MachineBasicBlock::iterator Filler,
                           MachineBasicBlock &MBB) {
  for (const MachineOperand &MO : Filler->operands()) {
    if (!MO.isReg() || !MO.isDef() || !MO.getReg())
      continue;
    MCPhysReg R = MO.getReg().asMCReg();
    if (!MBB.isLiveIn(R))
      MBB.addLiveIn(R);
  }
}

// ---------------------------------------------------------------------------
// Pass implementation
// ---------------------------------------------------------------------------

bool MicroBlazeDelaySlotFiller::runOnMachineFunction(MachineFunction &MF) {
  FilledBranches.clear();
  const TargetInstrInfo *TII =
      MF.getSubtarget<MicroBlazeSubtarget>().getInstrInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end(); ++I) {
      if (!I->hasDelaySlot())
        continue;

      bool Filled = false;
      if (searchBackward(MBB, I, TII)) {
        Filled = true;
      } else if (I->isTerminator() && searchSuccBBs(MBB, I, TII)) {
        Filled = true;
      }

      if (Filled) {
        FilledBranches.insert(&*I);
        ++FilledSlots;
        ++I; // skip the filler; outer loop advances past it
      } else if (unsigned NoDelayOpc = getNoDelayVariant(I->getOpcode())) {
        // No productive filler found but a no-delay form exists: swap in-place.
        I->setDesc(TII->get(NoDelayOpc));
        // No delay slot follows; do not advance I.
      } else {
        // No productive filler and no no-delay form (call or return).
        BuildMI(MBB, std::next(I), I->getDebugLoc(), TII->get(MicroBlaze::NOP));
        FilledBranches.insert(&*I);
        ++FilledSlots;
        ++I;
      }
      Changed = true;
    }
  }

  if (Changed)
    MF.getRegInfo().invalidateLiveness();
  return Changed;
}

bool MicroBlazeDelaySlotFiller::searchBackward(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator BranchIt,
    const TargetInstrInfo *TII) const {
  SmallSet<MCPhysReg, 8> BranchDefs, BranchUses;
  collectRegs(*BranchIt, BranchDefs, BranchUses);

  SmallSet<MCPhysReg, 8> DefsAfter, UsesAfter;

  if (BranchIt == MBB.begin())
    return false;

  auto I = BranchIt;
  while (I != MBB.begin()) {
    --I;
    MachineInstr &MI = *I;

    if (MI.isBranch() || MI.isCall() || MI.isReturn() || MI.hasDelaySlot())
      return false;

    if (MI.mayStore() || MI.hasUnmodeledSideEffects() ||
        MI.isInlineAsm() || MI.isPseudo())
      return false;

    SmallSet<MCPhysReg, 8> CandDefs, CandUses;
    collectRegs(MI, CandDefs, CandUses);

    if (overlaps(CandDefs, DefsAfter))  goto next;
    if (overlaps(CandDefs, UsesAfter))  goto next;
    if (overlaps(CandUses, DefsAfter))  goto next;
    if (overlaps(CandDefs, BranchDefs)) goto next;
    // Conservative backedge guard: do not define a register the branch reads.
    if (overlaps(CandDefs, BranchUses)) goto next;

    {
      MBB.splice(std::next(BranchIt), &MBB, I);
      ++UsefulSlots;
      return true;
    }

  next:
    for (MCPhysReg R : CandDefs) DefsAfter.insert(R);
    for (MCPhysReg R : CandUses) UsesAfter.insert(R);
  }
  return false;
}

MachineBasicBlock *
MicroBlazeDelaySlotFiller::selectSuccBB(MachineBasicBlock &MBB) const {
  if (MBB.succ_empty())
    return nullptr;
  auto &Prob =
      getAnalysis<MachineBranchProbabilityInfoWrapperPass>().getMBPI();
  MachineBasicBlock *S =
      *llvm::max_element(MBB.successors(),
                         [&](const MachineBasicBlock *A,
                             const MachineBasicBlock *B) {
                           return Prob.getEdgeProbability(&MBB, A) <
                                  Prob.getEdgeProbability(&MBB, B);
                         });
  return S->isEHPad() ? nullptr : S;
}

bool MicroBlazeDelaySlotFiller::examinePred(MachineBasicBlock &Pred,
                                             const MachineBasicBlock &Succ,
                                             SmallSet<MCPhysReg, 8> &BannedDefs,
                                             bool &HasMultipleSuccs,
                                             BB2BrMap &BrMap) const {
  // Walk backward through Pred's terminators looking for a branch to Succ.
  MachineInstr *BrMI = nullptr;
  for (auto I = Pred.rbegin(); I != Pred.rend(); ++I) {
    if (I->isDebugInstr() || I->isImplicitDef())
      continue;
    if (!I->isBranch())
      break; // no branch at end of block; Pred falls through to Succ

    bool TargetsSucc = false;
    for (const MachineOperand &MO : I->operands())
      if (MO.isMBB() && MO.getMBB() == &Succ) { TargetsSucc = true; break; }

    if (TargetsSucc) {
      // Must have an unoccupied delay slot to accept the clone.
      if (!I->hasDelaySlot() || FilledBranches.count(&*I))
        return false;
      BrMI = &*I;
      break;
    }
  }
  // BrMI == nullptr: Pred falls straight through to Succ (no direct branch).

  if (BrMI) {
    // The clone lives in BrMI's delay slot.  An unconditional branch's delay
    // slot executes only on the taken path (→ Succ), so no additional banning
    // is needed.  A conditional branch's delay slot executes on both the taken
    // path (→ Succ) and the fall-through path (→ OtherSucc), so we must ban
    // registers live-in to OtherSucc.  Mirrors MIPS: addLiveOut only for Cond.
    if (BrMI->isConditionalBranch()) {
      HasMultipleSuccs = true;
      for (const MachineBasicBlock *S : Pred.successors())
        if (S != &Succ)
          for (const auto &LI : S->liveins())
            BannedDefs.insert(MCPhysReg(LI.PhysReg));
    }
  } else {
    // Fall-through case (push_back).  Only proceed if Pred has no terminal
    // branches at all; if there is any branch (even to another target) the
    // clone would land in that branch's delay slot and require complex analysis.
    for (auto I = Pred.rbegin(); I != Pred.rend(); ++I) {
      if (I->isDebugInstr() || I->isImplicitDef())
        continue;
      if (I->isBranch())
        return false;
      break;
    }
  }

  BrMap[&Pred] = BrMI;
  return true;
}

bool MicroBlazeDelaySlotFiller::searchSuccBBs(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator Slot,
    const TargetInstrInfo *TII) const {
  MachineBasicBlock *SuccBB = selectSuccBB(MBB);
  if (!SuccBB)
    return false;

  // BannedDefs: registers the filler must not define — live-in to non-Succ
  // paths that would also execute the delay slot clone.
  SmallSet<MCPhysReg, 8> BannedDefs;
  bool HasMultipleSuccs = false;
  BB2BrMap BrMap;

  for (MachineBasicBlock *Pred : SuccBB->predecessors())
    if (!examinePred(*Pred, *SuccBB, BannedDefs, HasMultipleSuccs, BrMap))
      return false;

  // Also ban registers written by the branch itself (e.g. R15 for bralid).
  SmallSet<MCPhysReg, 8> BranchDefs, BranchUses;
  collectRegs(*Slot, BranchDefs, BranchUses);
  for (MCPhysReg R : BranchDefs)
    BannedDefs.insert(R);

  // Scan SuccBB forward for the first instruction safe to hoist.
  SmallSet<MCPhysReg, 8> DefsAfter, UsesAfter;
  for (auto I = SuccBB->begin(); I != SuccBB->end(); ++I) {
    MachineInstr &MI = *I;
    if (MI.isDebugInstr())
      continue;

    // Stop at anything that cannot be safely pre-executed.
    if (MI.isBranch() || MI.isCall() || MI.isReturn() || MI.hasDelaySlot() ||
        MI.mayStore() || MI.hasUnmodeledSideEffects() ||
        MI.isInlineAsm() || MI.isPseudo())
      break;

    SmallSet<MCPhysReg, 8> CandDefs, CandUses;
    collectRegs(MI, CandDefs, CandUses);

    bool Safe = !overlaps(CandDefs, BannedDefs) &&
                !overlaps(CandDefs, DefsAfter)  &&
                !overlaps(CandDefs, UsesAfter)  &&
                !overlaps(CandUses, DefsAfter)  &&
                !overlaps(CandDefs, BranchUses);

    if (Safe) {
      insertDelayFiller(I, BrMap);
      addLiveInRegs(I, *SuccBB);
      I->eraseFromParent();
      return true;
    }

    for (MCPhysReg R : CandDefs) DefsAfter.insert(R);
    for (MCPhysReg R : CandUses) UsesAfter.insert(R);
  }
  return false;
}

FunctionPass *llvm::createMicroBlazeDelaySlotFiller() {
  return new MicroBlazeDelaySlotFiller();
}
