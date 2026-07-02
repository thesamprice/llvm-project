//===-- DelaySlotFillerBase.h - Generic delay-slot filler ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Target-independent machinery for filling branch delay slots.  This is the
// reusable core factored out of the MicroBlaze delay-slot filler: a target
// subclasses DelaySlotFiller and implements the policy hooks describing its
// ISA's delay-slot contract (slot count, forbidden fillers, load-use hazards,
// no-delay branch variants, NOP encoding, optional pre-fill transforms).  The
// search / hoist / clone / bundle mechanism, register and memory hazard
// tracking, successor and join-block code motion, and liveness maintenance are
// all shared.
//
// The algorithm, per delayed terminator, tries three strategies in order:
//   1. searchBackward  — hoist a safe predecessor instruction from the block.
//   2. searchSuccBBs   — clone the hottest successor's first safe instruction
//                        into the delay slots of all of that successor's preds.
//   3. searchJoinBB    — for a branch diamond, MOVE the first safe instruction
//                        of the join block into the slot (runs on every path).
// On failure it either demotes to a no-delay branch form (getNoDelayOpcode) or
// inserts a NOP, per preferNopOverDemote.
//
// This file deliberately contains no target-specific enums or includes so it
// can later be lifted to llvm/CodeGen/ once a second target adopts it.  The
// getNumDelaySlots hook is the seam for targets with more than one slot (e.g. a
// two-slot return); the load-use and demote hooks are the seam for targets
// whose behavior depends on the selected pipeline.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_DELAYSLOTFILLERBASE_H
#define LLVM_LIB_TARGET_MICROBLAZE_DELAYSLOTFILLERBASE_H

#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/IR/DebugLoc.h"

namespace llvm {

class MachineInstr;
class TargetInstrInfo;
class TargetRegisterInfo;

/// Maps each predecessor block to the branch instruction whose delay slot will
/// receive the cloned filler, or nullptr if the predecessor falls straight
/// through (no branch) and the clone should be appended to its end.
using BB2BrMap = SmallDenseMap<MachineBasicBlock *, MachineInstr *, 2>;

/// Target-independent branch delay-slot filler.  Subclass and override the
/// policy hooks below; the search mechanism is provided.
class DelaySlotFiller : public MachineFunctionPass {
protected:
  // Set once per function in runOnMachineFunction; used by all helpers.
  mutable const TargetInstrInfo *TII = nullptr;
  mutable const TargetRegisterInfo *TRI = nullptr;

  explicit DelaySlotFiller(char &ID) : MachineFunctionPass(ID) {}

  // === Target policy hooks =================================================

  /// Number of delay slots that Br requires.  The shared mechanism currently
  /// fills a single slot; targets with N>1 (e.g. a two-slot return) are the
  /// reason this is a hook rather than a constant.
  virtual unsigned getNumDelaySlots(const MachineInstr &Br) const { return 1; }

  /// True if MI cannot occupy a delay slot because it encodes to more than one
  /// machine word (e.g. it needs an IMM prefix word).  Such an instruction is
  /// skipped over during the search but never placed in the slot.
  virtual bool needsMultiWordEncoding(const MachineInstr &MI) const {
    return false;
  }

  /// True if MI is a plain load (used to gate the load-use hazard check).
  /// Default: reads memory and does not write it.
  virtual bool isLoadInstruction(const MachineInstr &MI) const;

  /// True if placing Filler in the delay slot of a branch that immediately
  /// follows Load would create a load-use hazard.  Default: no hazard.
  virtual bool hasLoadUseHazard(const MachineInstr &Filler,
                                const MachineInstr &Load) const {
    return false;
  }

  /// The no-delay-slot equivalent opcode for delayed branch Opc, or 0 if the
  /// ISA has none (e.g. calls/returns that always carry a slot).
  virtual unsigned getNoDelayOpcode(unsigned Opc) const { return 0; }

  /// Emit a NOP into MBB before Pos.
  virtual void insertNop(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator Pos,
                         const DebugLoc &DL) const = 0;

  /// When no filler is found and a no-delay form exists, choose between keeping
  /// the delay form with a NOP (return true) and demoting to the no-delay form
  /// (return false).  IsBackwardBranch is true for loop back-edges.  Targets may
  /// override to make this pipeline-dependent; the default keeps the delay form
  /// with a NOP for backward branches (taken nearly every iteration) and demotes
  /// forward branches (cheaper not-taken).
  virtual bool preferNopOverDemote(const MachineInstr &Br,
                                   bool IsBackwardBranch) const {
    return IsBackwardBranch;
  }

  /// Optional target-specific transforms run once before the generic filler
  /// (e.g. hoisting copies or sinking address increments to expose slots).
  /// Returns true if the function was modified.
  virtual bool runPreFillTransforms(MachineFunction &MF) { return false; }

  // === Shared mechanism ====================================================

  /// Search backward within MBB from BranchIt for an instruction safe to hoist
  /// into the delay slot.  On success, splices it in and returns true.
  bool searchBackward(MachineBasicBlock &MBB,
                      MachineBasicBlock::iterator BranchIt) const;

  /// Search the hottest successor of MBB for an instruction that can be cloned
  /// into the delay slots of all predecessors of that successor.
  bool searchSuccBBs(MachineBasicBlock &MBB,
                     MachineBasicBlock::iterator Slot) const;

  /// Search for a "join" successor reachable from every other successor of MBB
  /// and MOVE its first safe instruction into the delay slot.
  bool searchJoinBB(MachineBasicBlock &MBB,
                    MachineBasicBlock::iterator Slot) const;

  /// Pick the highest-probability non-EH-pad successor of MBB, or nullptr.
  MachineBasicBlock *selectSuccBB(MachineBasicBlock &MBB) const;

  /// Examine predecessor Pred of Succ; record whether Pred has an unoccupied
  /// delay slot targeting Succ (or falls through), accumulate banned defs from
  /// other successor live-ins, and add to BrMap.  Returns false if Pred cannot
  /// accept a clone.
  bool examinePred(MachineBasicBlock &Pred, const MachineBasicBlock &Succ,
                   BitVector &BannedDefs, bool &HasMultipleSuccs,
                   BB2BrMap &BrMap) const;

public:
  bool runOnMachineFunction(MachineFunction &MF) override;

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().setNoVRegs();
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_DELAYSLOTFILLERBASE_H
