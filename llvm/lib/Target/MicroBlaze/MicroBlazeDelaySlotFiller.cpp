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
// The filler scans backward from each delay-slot branch within the same basic
// block for the first instruction that is safe to hoist.  If none is found:
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
#include "llvm/ADT/SmallSet.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_INSTRINFO_ENUM
#include "MicroBlazeGenInstrInfo.inc"

using namespace llvm;

#define DEBUG_TYPE "microblaze-delay-slot-filler"

namespace {

struct MicroBlazeDelaySlotFiller : public MachineFunctionPass {
  static char ID;

  MicroBlazeDelaySlotFiller() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return "MicroBlaze delay slot filler";
  }
};

} // namespace

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

// Return true if Sets A and B share any element.
static bool overlaps(const SmallSet<MCPhysReg, 8> &A,
                     const SmallSet<MCPhysReg, 8> &B) {
  for (MCPhysReg R : A)
    if (B.count(R))
      return true;
  return false;
}

// Try to find an instruction earlier in MBB that can be legally moved into the
// delay slot immediately after BranchIt.  If found, splice it there and return
// true.  Otherwise return false.
static bool tryFillSlot(MachineBasicBlock &MBB,
                        MachineBasicBlock::iterator BranchIt,
                        const TargetInstrInfo *TII) {
  // Registers explicitly defined by the branch (e.g. R15 for brald/bralid).
  // A hoisted instruction must not write these — it would clobber the branch's
  // output (the saved return address for calls).
  SmallSet<MCPhysReg, 8> BranchDefs, BranchUses;
  collectRegs(*BranchIt, BranchDefs, BranchUses);

  // Hazard sets that grow as we walk backward past each instruction between the
  // current candidate and the branch.
  SmallSet<MCPhysReg, 8> DefsAfter; // regs written by instructions after cand
  SmallSet<MCPhysReg, 8> UsesAfter; // regs read    by instructions after cand

  if (BranchIt == MBB.begin())
    return false;

  auto I = BranchIt;
  while (I != MBB.begin()) {
    --I;
    MachineInstr &MI = *I;

    // Stop at anything that cannot be safely bypassed.
    if (MI.isBranch() || MI.isCall() || MI.isReturn() || MI.hasDelaySlot())
      return false;

    // These cannot be safely reordered or reasoned about.
    if (MI.mayStore() || MI.hasUnmodeledSideEffects() ||
        MI.isInlineAsm() || MI.isPseudo())
      return false;

    // Loads are generally fine to hoist as long as register hazards are clear.
    // (MicroBlaze memory is not reordered relative to other instructions in the
    // delay slot — the delay slot just executes one cycle later.)

    SmallSet<MCPhysReg, 8> CandDefs, CandUses;
    collectRegs(MI, CandDefs, CandUses);

    // WAW hazard: MI defines a reg that a later instruction also defines.
    // Moving MI after those instructions would leave the wrong final value.
    if (overlaps(CandDefs, DefsAfter))
      goto next;

    // WAR hazard: MI defines a reg that a later instruction reads.
    // Moving MI after those instructions would change what they read.
    if (overlaps(CandDefs, UsesAfter))
      goto next;

    // RAW hazard: MI reads a reg that a later instruction writes.
    // Moving MI after those instructions would make MI read a new value.
    if (overlaps(CandUses, DefsAfter))
      goto next;

    // MI must not define a reg the branch also defines (e.g. both writing R15).
    if (overlaps(CandDefs, BranchDefs))
      goto next;

    // Conservative backedge guard: do not hoist an instruction that defines a
    // register the branch reads.  On a loop backedge the branch would read
    // the delay-slot value on the *next* iteration but an uninitialized value
    // on the *first* iteration.
    if (overlaps(CandDefs, BranchUses))
      goto next;

    {
      // MI is safe to hoist: splice it into the delay slot position.
      auto InsertPt = std::next(BranchIt);
      MBB.splice(InsertPt, &MBB, I);
      return true;
    }

  next:
    // Accumulate hazards so we can check candidates further back.
    for (MCPhysReg R : CandDefs) DefsAfter.insert(R);
    for (MCPhysReg R : CandUses) UsesAfter.insert(R);
  }
  return false;
}

bool MicroBlazeDelaySlotFiller::runOnMachineFunction(MachineFunction &MF) {
  const TargetInstrInfo *TII =
      MF.getSubtarget<MicroBlazeSubtarget>().getInstrInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end(); ++I) {
      if (!I->hasDelaySlot())
        continue;

      if (tryFillSlot(MBB, I, TII)) {
        // Successfully hoisted an instruction; the filler is now at next(I).
        ++I;
      } else if (unsigned NoDelayOpc = getNoDelayVariant(I->getOpcode())) {
        // No productive filler found, but a no-delay form exists.
        // Swap the opcode in-place — operand lists are identical between the
        // delay and no-delay variants, only the encoding's delay bit differs.
        I->setDesc(TII->get(NoDelayOpc));
        // No delay slot instruction follows; do not advance I.
      } else {
        // No productive filler and no no-delay form (call or return).
        // Insert a NOP into the delay slot.
        auto InsertPt = std::next(I);
        BuildMI(MBB, InsertPt, I->getDebugLoc(), TII->get(MicroBlaze::NOP));
        ++I;
      }
      Changed = true;
    }
  }
  return Changed;
}

char MicroBlazeDelaySlotFiller::ID = 0;

FunctionPass *llvm::createMicroBlazeDelaySlotFiller() {
  return new MicroBlazeDelaySlotFiller();
}
