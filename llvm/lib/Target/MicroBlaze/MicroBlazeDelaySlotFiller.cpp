//===-- MicroBlazeDelaySlotFiller.cpp - Fill MicroBlaze delay slots -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MicroBlaze branches and returns have a one-instruction delay slot (UG984 §2).
// When a taken branch uses a delay slot (the "D" form, e.g. BNED vs BNE), only
// the fetch stage is flushed; the instruction already in the decode stage (the
// delay slot) is allowed to complete before control transfers to the branch
// target.  This reduces the branch penalty from two clock cycles to one.
//
// UG984 §2 delay-slot restrictions
// ---------------------------------
// The following instructions are FORBIDDEN in a delay slot:
//
//   IMM / IMML  — An instruction that requires an IMM prefix encodes as two
//                 consecutive 32-bit words.  The delay slot holds exactly one
//                 word, so a two-word instruction there is broken in two ways:
//                 (a) for call delay slots, bralid stores PC+4 past the IMM
//                     word; after the callee returns, the actual instruction
//                     word is silently skipped; (b) for branch delay slots, the
//                 IMM latch leaks into the first instruction at the branch
//                 target.  Enforced by needsImmPrefix() in both scan loops.
//                 (IMML is the 64-bit variant used by MB-X; not yet in this
//                 backend, but needsImmPrefix() would catch it the same way.)
//
//   Branch      — A branch instruction in a delay slot is architecturally
//                 undefined (UG984 §2).  Enforced by MI.isBranch() early in
//                 both scan loops; the check also stops the backward scan from
//                 crossing any earlier branch boundary.
//
//   Break       — BRK / BRKI transfer to the break handler; placing one in a
//                 delay slot is architecturally prohibited (UG984 §2).  Both
//                 instructions carry hasSideEffects=1 in their TableGen defs,
//                 which sets MCID::UnmodeledSideEffects; consequently
//                 MI.hasUnmodeledSideEffects() returns true and excludes them
//                 from both scan loops.
//
// Recoverable exceptions (e.g. unaligned load/store) ARE allowed in delay slots
// per UG984 §2.  When such an exception fires in a delay slot the hardware sets
// ESR[DS]=1 and stores the branch target address in BTR; the exception handler
// must restart from BTR rather than the usual PC+4.  LLVM does not model this
// special restart path, but the instructions themselves are correctly placed —
// the hardware exception mechanism works regardless of who placed the instruction.
//
// Condition evaluation and register hazards
// -----------------------------------------
// The branch evaluates its condition register *before* the delay slot runs, so
// the delay slot may freely write registers that the branch reads — EXCEPT for
// loop backedges where the same branch fires again on the next iteration.  To be
// safe we never hoist an instruction that defines a register the branch
// *explicitly* reads.
//
// For call (bralid/brald) and return (rtsd/rtid/rtbd/rted) instructions the
// implicit-use operands model calling-convention data flow (argument and return-
// value registers), not actual reads performed by the instruction itself.
// Hoisting an argument setup (e.g., addik r5, r19, -1 before bralid r15, fib)
// into the delay slot is safe because the slot executes before the callee reads
// r5.  We therefore guard only against *explicit* register uses of call/return
// instructions; the implicit-use argument registers are excluded from the guard.
//
// Fill strategies
// ---------------
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
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineBranchProbabilityInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineInstrBundle.h"
#include "llvm/CodeGen/PseudoSourceValue.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/MC/MCRegisterInfo.h"

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
  // Set once per function in runOnMachineFunction; used by all helpers.
  mutable const TargetRegisterInfo *TRI = nullptr;

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

  /// Search for a "join" successor — a direct successor of MBB that is also
  /// reachable (as a direct successor) from every other successor of MBB.
  /// For a branch diamond (e.g. ABS: headMBB → {negMBB, sinkMBB},
  /// negMBB → sinkMBB), sinkMBB is the join.  The first safe instruction
  /// from the join block is MOVED (not cloned) into the delay slot.  No copy
  /// is inserted in intermediate blocks because the delay slot executes on
  /// every path from MBB before reaching the join block, mirroring the
  /// original placement.
  bool searchJoinBB(MachineBasicBlock &MBB,
                    MachineBasicBlock::iterator Slot,
                    const TargetInstrInfo *TII) const;

  /// Pick the highest-probability non-EH-pad successor of MBB, or nullptr.
  MachineBasicBlock *selectSuccBB(MachineBasicBlock &MBB) const;

  /// Examine predecessor Pred of Succ.  Records whether Pred has an unoccupied
  /// delay slot that branches to Succ (or falls through), accumulates banned
  /// register defs from other successor live-ins, and adds to BrMap.
  /// Returns false if Pred cannot accept a clone.
  bool examinePred(MachineBasicBlock &Pred, const MachineBasicBlock &Succ,
                   BitVector &BannedDefs, bool &HasMultipleSuccs,
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

// Collect all register numbers read/written by MI into Defs/Uses, expanding
// each register to its full alias set via MCRegAliasIterator.  This covers
// future sub-register pairs (e.g. 64-bit FP or mulh result pairs) correctly.
static void collectRegs(const MachineInstr &MI,
                        const TargetRegisterInfo *TRI,
                        BitVector &Defs, BitVector &Uses) {
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isReg() || !MO.getReg().isPhysical())
      continue;
    MCPhysReg R = MO.getReg().asMCReg();
    for (MCRegAliasIterator AI(R, TRI, /*IncludeSelf=*/true); AI.isValid();
         ++AI) {
      if (MO.isDef())
        Defs.set(*AI);
      else
        Uses.set(*AI);
    }
  }
}

// Extract the fixed-stack frame index from a load/store's MachineMemOperand.
// Returns true and sets FI if the instruction accesses a known frame slot.
// Fixed stack slots are non-aliasing with each other by construction, so two
// instructions with different FIs can always be safely reordered.
static bool getFrameIndex(const MachineInstr &MI, int &FI) {
  for (const MachineMemOperand *MMO : MI.memoperands())
    if (const auto *PSV = dyn_cast_or_null<FixedStackPseudoSourceValue>(
            MMO->getPseudoValue())) {
      FI = PSV->getFrameIndex();
      return true;
    }
  return false;
}

// True if MI would need a preceding IMM prefix word when encoded.
// UG984 §2 explicitly forbids IMM (and its 64-bit variant IMML) in a delay
// slot.  Beyond the architectural prohibition, a two-word encoding is
// structurally broken in a one-word slot: for call delay slots the bralid
// return address points past the IMM word, so the actual instruction word is
// silently skipped after the callee returns; for branch delay slots the IMM
// latch leaks into the first instruction at the branch target.
static bool needsImmPrefix(const MachineInstr &MI) {
  for (const MachineOperand &MO : MI.operands()) {
    switch (MO.getType()) {
    case MachineOperand::MO_GlobalAddress:
    case MachineOperand::MO_ExternalSymbol:
    case MachineOperand::MO_ConstantPoolIndex:
    case MachineOperand::MO_JumpTableIndex:
    case MachineOperand::MO_BlockAddress:
      return true;
    case MachineOperand::MO_Immediate:
      if (MO.getImm() < -32768 || MO.getImm() > 32767)
        return true;
      break;
    default:
      break;
    }
  }
  return false;
}

// True if any bit is set in both A and B.
static bool overlaps(const BitVector &A, const BitVector &B) {
  for (unsigned I : A.set_bits())
    if (B.test(I))
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
      MachineBasicBlock::iterator BrIt(BrMI);
      Pred->insert(std::next(BrIt), Clone);
      MIBundleBuilder(*Pred, BrIt.getInstrIterator(),
                      std::next(BrIt.getInstrIterator(), 2));
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
  TRI = MF.getSubtarget<MicroBlazeSubtarget>().getRegisterInfo();
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
      } else if (I->isTerminator() && searchJoinBB(MBB, I, TII)) {
        Filled = true;
      }

      if (Filled) {
        ++FilledSlots;
        // Filler is bundled with the branch inside searchBackward or
        // insertDelayFiller; bundle_iterator naturally skips the bundle body.
      } else if (unsigned NoDelayOpc = getNoDelayVariant(I->getOpcode())) {
        // No productive filler found and a no-delay form exists.
        // For backward branches (loop back-edges), the branch is taken on
        // nearly every iteration: non-D costs 3 cycles taken vs. D+NOP costs
        // 2 cycles, so keep D-form and insert a NOP instead.
        // For forward branches (exit checks), non-D costs 1 cycle not-taken
        // vs. D+NOP costs 2 cycles, so demoting to non-D is a net win.
        bool IsBackward = false;
        for (const MachineOperand &MO : I->operands()) {
          if (MO.isMBB() &&
              MO.getMBB()->getNumber() <= MBB.getNumber()) {
            IsBackward = true;
            break;
          }
        }
        if (IsBackward) {
          BuildMI(MBB, std::next(I), I->getDebugLoc(), TII->get(MicroBlaze::NOP));
          MIBundleBuilder(MBB, I.getInstrIterator(),
                          std::next(I.getInstrIterator(), 2));
          ++FilledSlots;
        } else {
          I->setDesc(TII->get(NoDelayOpc));
        }
        // No delay slot follows; do not advance I.
      } else {
        // No productive filler and no no-delay form (call or return): insert NOP
        // and bundle it so the backward scanner cannot steal it for another slot.
        BuildMI(MBB, std::next(I), I->getDebugLoc(), TII->get(MicroBlaze::NOP));
        MIBundleBuilder(MBB, I.getInstrIterator(),
                        std::next(I.getInstrIterator(), 2));
        ++FilledSlots;
        // bundle_iterator's operator++ skips the bundled NOP automatically.
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
  const auto *MBII = static_cast<const MicroBlazeInstrInfo *>(TII);
  unsigned NumRegs = TRI->getNumRegs();
  BitVector BranchDefs(NumRegs), BranchUses(NumRegs);
  collectRegs(*BranchIt, TRI, BranchDefs, BranchUses);

  // For calls and returns, implicit operands are argument/return-value registers
  // that model calling-convention data flow rather than instruction reads.
  // Build BranchGuardUses from explicit uses only so hoisting a last-argument
  // setup (e.g., addik r5, r19, -1 before bralid r15, fib) is permitted.
  BitVector BranchGuardUses(NumRegs);
  if (BranchIt->isCall() || BranchIt->isReturn()) {
    for (const MachineOperand &MO : BranchIt->explicit_uses()) {
      if (!MO.isReg() || !MO.getReg().isPhysical())
        continue;
      for (MCRegAliasIterator AI(MO.getReg().asMCReg(), TRI,
                                 /*IncludeSelf=*/true);
           AI.isValid(); ++AI)
        BranchGuardUses.set(*AI);
    }
  } else {
    BranchGuardUses = BranchUses;
  }

  BitVector DefsAfter(NumRegs), UsesAfter(NumRegs);
  BitVector CandDefs(NumRegs), CandUses(NumRegs);

  // Memory alias state for the backward scan.  We track which fixed stack
  // frame indices have been stored to so that loads from other frame slots
  // can still be hoisted past those stores.
  SmallSet<int, 4> StoredFIs;
  bool SeenNoObjStore = false; // store to unknown / non-frame address

  if (BranchIt == MBB.begin())
    return false;

  auto I = BranchIt;
  while (I != MBB.begin()) {
    --I;
    MachineInstr &MI = *I;

    // UG984 §2: branch and break instructions are forbidden in delay slots.
    // isBranch() covers all branch/jump terminators.  Break instructions
    // (BRK, BRKI) are caught below by hasUnmodeledSideEffects() because their
    // TableGen defs carry hasSideEffects=1 → MCID::UnmodeledSideEffects.
    if (MI.isBranch() || MI.isCall() || MI.isReturn() || MI.hasDelaySlot())
      return false;

    if (MI.hasUnmodeledSideEffects() || MI.isInlineAsm() || MI.isPseudo())
      return false;

    // UG984 §2: IMM/IMML are forbidden in delay slots (see needsImmPrefix()).
    if (needsImmPrefix(MI))
      goto next;

    CandDefs.reset();
    CandUses.reset();
    collectRegs(MI, TRI, CandDefs, CandUses);

    // Stores cannot be candidates (side effects), but do not end the scan.
    // Record the store's memory target so load candidates can be alias-checked.
    if (MI.mayStore()) {
      int FI;
      if (getFrameIndex(MI, FI))
        StoredFIs.insert(FI);
      else
        SeenNoObjStore = true;
      goto next;
    }

    // Register hazard checks.
    if (overlaps(CandDefs, DefsAfter))  goto next;
    if (overlaps(CandDefs, UsesAfter))  goto next;
    if (overlaps(CandUses, DefsAfter))  goto next;
    if (overlaps(CandDefs, BranchDefs)) goto next;
    // Backedge guard: do not define a register the branch explicitly reads.
    // BranchGuardUses excludes implicit (arg/return-value) regs for calls/returns.
    if (overlaps(CandDefs, BranchGuardUses)) goto next;

    // Memory alias check for loads: allow hoisting a frame-slot load past
    // stores to *different* frame slots; block everything else conservatively.
    if (MI.mayLoad()) {
      int LoadFI;
      if (!getFrameIndex(MI, LoadFI)) {
        // Unknown address — block if any store has been seen.
        if (SeenNoObjStore || !StoredFIs.empty())
          goto next;
      } else {
        // Known frame slot — block only if that slot was stored to, or if an
        // unknown store may have written to it.
        if (SeenNoObjStore || StoredFIs.count(LoadFI))
          goto next;
      }
    }

    // Explicit load-use hazard check (UG984 §2, IIC_LD=2 in Schedule.td).
    // For each load instruction already scanned between this candidate and the
    // branch, isSafeInLoadDelaySlot verifies the candidate can safely fill the
    // delay slot.  The branch occupies one pipeline stage between load and
    // delay slot, satisfying the 2-cycle latency requirement — so this check
    // always passes for MicroBlaze.  It exists to document the invariant in a
    // form that can be independently tested and relaxed if needed.
    for (auto J = std::next(I); J != BranchIt; ++J)
      if (MBII->isLoadInstruction(*J) && !MBII->isSafeInLoadDelaySlot(MI, *J))
        goto next;

    {
      MBB.splice(std::next(BranchIt), &MBB, I);
      MIBundleBuilder(MBB, BranchIt.getInstrIterator(),
                      std::next(BranchIt.getInstrIterator(), 2));
      ++UsefulSlots;
      return true;
    }

  next:
    DefsAfter |= CandDefs;
    UsesAfter |= CandUses;
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
                                             BitVector &BannedDefs,
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
      if (!I->hasDelaySlot() || I->isBundledWithSucc())
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
      for (const MachineBasicBlock *S : Pred.successors()) {
        if (S == &Succ)
          continue;
        for (const auto &LI : S->liveins())
          for (MCRegAliasIterator AI(LI.PhysReg, TRI, /*IncludeSelf=*/true);
               AI.isValid(); ++AI)
            BannedDefs.set(*AI);
      }
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

  unsigned NumRegs = TRI->getNumRegs();

  // BannedDefs: registers the filler must not define — live-in to non-Succ
  // paths that would also execute the delay slot clone.
  BitVector BannedDefs(NumRegs);
  bool HasMultipleSuccs = false;
  BB2BrMap BrMap;

  for (MachineBasicBlock *Pred : SuccBB->predecessors())
    if (!examinePred(*Pred, *SuccBB, BannedDefs, HasMultipleSuccs, BrMap))
      return false;

  // Also ban registers written by the branch itself (e.g. R15 for bralid).
  BitVector BranchDefs(NumRegs), BranchUses(NumRegs);
  collectRegs(*Slot, TRI, BranchDefs, BranchUses);
  BannedDefs |= BranchDefs;

  // Same explicit-only guard as searchBackward: for calls/returns, exclude
  // implicit argument/return-value registers from the use guard.
  BitVector BranchGuardUses(NumRegs);
  if (Slot->isCall() || Slot->isReturn()) {
    for (const MachineOperand &MO : Slot->explicit_uses()) {
      if (!MO.isReg() || !MO.getReg().isPhysical())
        continue;
      for (MCRegAliasIterator AI(MO.getReg().asMCReg(), TRI,
                                 /*IncludeSelf=*/true);
           AI.isValid(); ++AI)
        BranchGuardUses.set(*AI);
    }
  } else {
    BranchGuardUses = BranchUses;
  }

  // Scan SuccBB forward for the first instruction safe to hoist.
  BitVector DefsAfter(NumRegs), UsesAfter(NumRegs);
  BitVector CandDefs(NumRegs), CandUses(NumRegs);
  for (auto I = SuccBB->begin(); I != SuccBB->end(); ++I) {
    MachineInstr &MI = *I;
    if (MI.isDebugInstr())
      continue;

    // Stop at anything that cannot be safely pre-executed.
    // UG984 §2: branch and break instructions are forbidden (isBranch() and
    // hasUnmodeledSideEffects() cover them; see searchBackward comment).
    if (MI.isBranch() || MI.isCall() || MI.isReturn() || MI.hasDelaySlot() ||
        MI.mayStore() || MI.hasUnmodeledSideEffects() ||
        MI.isInlineAsm() || MI.isPseudo())
      break;

    // UG984 §2: IMM/IMML are forbidden in delay slots (see needsImmPrefix()).
    // Keep scanning — a later instruction in SuccBB may be safe.
    if (needsImmPrefix(MI)) {
      CandDefs.reset();
      CandUses.reset();
      collectRegs(MI, TRI, CandDefs, CandUses);
      DefsAfter |= CandDefs;
      UsesAfter |= CandUses;
      continue;
    }

    CandDefs.reset();
    CandUses.reset();
    collectRegs(MI, TRI, CandDefs, CandUses);

    bool Safe = !overlaps(CandDefs, BannedDefs) &&
                !overlaps(CandDefs, DefsAfter)  &&
                !overlaps(CandDefs, UsesAfter)  &&
                !overlaps(CandUses, DefsAfter)  &&
                !overlaps(CandDefs, BranchGuardUses);

    if (Safe) {
      insertDelayFiller(I, BrMap);
      addLiveInRegs(I, *SuccBB);
      I->eraseFromParent();
      return true;
    }

    DefsAfter |= CandDefs;
    UsesAfter |= CandUses;
  }
  return false;
}

bool MicroBlazeDelaySlotFiller::searchJoinBB(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator Slot,
    const TargetInstrInfo *TII) const {
  // Identify a "join" successor: a direct successor J of MBB such that every
  // other direct successor of MBB also has J as a direct successor.
  // For the ABS diamond (headMBB → {negMBB, sinkMBB}, negMBB → sinkMBB),
  // sinkMBB is the join block.  Moving the first safe instruction from J to
  // the delay slot is semantically equivalent to its original placement because
  // the delay slot executes on all paths from MBB before reaching J.
  MachineBasicBlock *JoinBB = nullptr;
  for (MachineBasicBlock *Succ : MBB.successors()) {
    bool IsJoin = true;
    for (MachineBasicBlock *Other : MBB.successors()) {
      if (Other == Succ)
        continue;
      if (!llvm::is_contained(Other->successors(), Succ)) {
        IsJoin = false;
        break;
      }
    }
    if (IsJoin) {
      JoinBB = Succ;
      break;
    }
  }
  if (!JoinBB)
    return false;

  // The moved instruction is REMOVED from JoinBB and placed only in MBB's delay
  // slot, so it must execute exactly once on every path that enters JoinBB and
  // never on a path that bypasses JoinBB.  Require a clean diamond:
  //
  //   * every intermediate block (a successor of MBB other than JoinBB) has MBB
  //     as its only predecessor and JoinBB as its only successor — otherwise the
  //     moved instruction is either skipped (extra entry that bypasses the delay
  //     slot) or runs spuriously (extra exit that never reaches JoinBB);
  //   * JoinBB is entered only from MBB (the taken edge) or an intermediate
  //     block — no external predecessor (loop backedge, shared label, goto)
  //     would otherwise skip the moved instruction.
  for (MachineBasicBlock *Other : MBB.successors()) {
    if (Other == JoinBB)
      continue;
    if (Other->pred_size() != 1 || Other->succ_size() != 1 ||
        *Other->succ_begin() != JoinBB)
      return false;
  }
  for (MachineBasicBlock *Pred : JoinBB->predecessors()) {
    if (Pred == &MBB)
      continue;
    if (Pred != JoinBB && llvm::is_contained(MBB.successors(), Pred))
      continue; // intermediate successor of MBB → also runs the delay slot
    return false;
  }

  unsigned NumRegs = TRI->getNumRegs();

  // Ban registers touched by instructions in intermediate blocks (those between
  // MBB and JoinBB that are NOT JoinBB itself).  The delay slot runs before
  // those blocks, so moving an instruction past them must respect:
  //   - WAR: moved def must not clobber a register an intermediate block READS;
  //   - RAW: moved use must not read a register an intermediate block WRITES
  //          (the moved instr would see the stale, pre-block value);
  //   - WAW: moved def must not race a register an intermediate block WRITES
  //          (the block's later write would win on the fall-through path).
  // BannedDefs collects intermediate READS; InterDefs collects intermediate
  // WRITES.  We use explicit instruction reads (not live-ins) for BannedDefs to
  // avoid false bans on registers that merely pass through an intermediate block
  // unused (e.g. the pointer register in an ABS negation block running RSUBK).
  BitVector BannedDefs(NumRegs);
  BitVector InterDefs(NumRegs);
  bool InterHasStore = false;
  for (MachineBasicBlock *Other : MBB.successors()) {
    if (Other == JoinBB)
      continue;
    for (const MachineInstr &MI : *Other) {
      if (MI.mayStore())
        InterHasStore = true;
      for (const MachineOperand &MO : MI.operands()) {
        if (!MO.isReg() || !MO.getReg().isPhysical())
          continue;
        BitVector &Target = MO.isDef() ? InterDefs : BannedDefs;
        for (MCRegAliasIterator AI(MO.getReg().asMCReg(), TRI,
                                   /*IncludeSelf=*/true);
             AI.isValid(); ++AI)
          Target.set(*AI);
      }
    }
  }

  // Also ban registers written by the branch itself.
  BitVector BranchDefs(NumRegs), BranchUses(NumRegs);
  collectRegs(*Slot, TRI, BranchDefs, BranchUses);
  BannedDefs |= BranchDefs;

  // Backedge guard: do not define a register the branch explicitly reads.
  // (Same logic as searchBackward / searchSuccBBs.)
  BitVector BranchGuardUses(NumRegs);
  if (Slot->isCall() || Slot->isReturn()) {
    for (const MachineOperand &MO : Slot->explicit_uses()) {
      if (!MO.isReg() || !MO.getReg().isPhysical())
        continue;
      for (MCRegAliasIterator AI(MO.getReg().asMCReg(), TRI,
                                 /*IncludeSelf=*/true);
           AI.isValid(); ++AI)
        BranchGuardUses.set(*AI);
    }
  } else {
    BranchGuardUses = BranchUses;
  }

  // Scan JoinBB forward for the first instruction safe to move.
  BitVector DefsAfter(NumRegs), UsesAfter(NumRegs);
  BitVector CandDefs(NumRegs), CandUses(NumRegs);
  for (auto I = JoinBB->begin(); I != JoinBB->end(); ++I) {
    MachineInstr &MI = *I;
    if (MI.isDebugInstr())
      continue;

    if (MI.isBranch() || MI.isCall() || MI.isReturn() || MI.hasDelaySlot() ||
        MI.mayStore() || MI.hasUnmodeledSideEffects() ||
        MI.isInlineAsm() || MI.isPseudo())
      break;

    // IMM-prefixed instructions are forbidden in delay slots (UG984 §2).
    if (needsImmPrefix(MI)) {
      CandDefs.reset();
      CandUses.reset();
      collectRegs(MI, TRI, CandDefs, CandUses);
      DefsAfter |= CandDefs;
      UsesAfter |= CandUses;
      continue;
    }

    CandDefs.reset();
    CandUses.reset();
    collectRegs(MI, TRI, CandDefs, CandUses);

    bool Safe = !overlaps(CandDefs, BannedDefs)       &&  // WAR vs interm. reads
                !overlaps(CandUses, InterDefs)         &&  // RAW vs interm. writes
                !overlaps(CandDefs, InterDefs)         &&  // WAW vs interm. writes
                !overlaps(CandDefs, BranchGuardUses)  &&
                !overlaps(CandDefs, DefsAfter)         &&
                !overlaps(CandDefs, UsesAfter)         &&
                !overlaps(CandUses, DefsAfter)         &&
                !(MI.mayLoad() && InterHasStore);          // memory RAW: interm. store may alias

    if (Safe) {
      // MOVE MI from JoinBB into the delay slot.  No clone is needed because
      // the delay slot already executes on every path from MBB before reaching
      // JoinBB, so correctness is preserved without duplicating the instruction.
      //
      // addLiveInRegs must be called while I is still a valid iterator in JoinBB
      // (before remove+insert invalidates the JoinBB context for I).
      addLiveInRegs(I, *JoinBB);
      JoinBB->remove(&MI);
      MBB.insert(std::next(Slot), &MI);
      MIBundleBuilder(MBB, Slot.getInstrIterator(),
                      std::next(Slot.getInstrIterator(), 2));
      ++UsefulSlots;
      return true;
    }

    DefsAfter |= CandDefs;
    UsesAfter |= CandUses;
  }
  return false;
}

FunctionPass *llvm::createMicroBlazeDelaySlotFiller() {
  return new MicroBlazeDelaySlotFiller();
}
