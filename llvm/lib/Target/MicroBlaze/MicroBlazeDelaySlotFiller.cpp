//===-- MicroBlazeDelaySlotFiller.cpp - Fill MicroBlaze delay slots -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MicroBlaze branches and returns have a one-instruction delay slot.
// This pass inserts a NOP into every unfilled delay slot.  Slot filling
// (hoisting the preceding instruction into the slot) is left as a
// follow-on optimization.
//
//===----------------------------------------------------------------------===//

#include "MicroBlaze.h"
#include "MicroBlazeInstrInfo.h"
#include "MicroBlazeSubtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_ENUM
#include "MicroBlazeGenInstrInfo.inc"

using namespace llvm;

#define DEBUG_TYPE "microblaze-delay-slot-filler"

namespace {

struct MicroBlazeDelaySlotFiller : public MachineFunctionPass {
  static char ID;

  MicroBlazeDelaySlotFiller() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const TargetInstrInfo *TII =
        MF.getSubtarget<MicroBlazeSubtarget>().getInstrInfo();
    bool Changed = false;

    for (MachineBasicBlock &MBB : MF) {
      for (auto I = MBB.begin(); I != MBB.end(); ++I) {
        if (!I->hasDelaySlot())
          continue;

        // Delay slot is the instruction immediately following the branch/return.
        // Insert a NOP there.  The NOP executes in the delay slot before the
        // branch target is entered (per UG984 Ch.5 delay-slot semantics).
        auto InsertPt = std::next(I);
        BuildMI(MBB, InsertPt, DebugLoc(), TII->get(MicroBlaze::NOP));
        Changed = true;

        // Skip over the newly inserted NOP so we don't revisit it.
        ++I;
      }
    }
    return Changed;
  }

  StringRef getPassName() const override {
    return "MicroBlaze delay slot filler";
  }
};

} // namespace

char MicroBlazeDelaySlotFiller::ID = 0;

FunctionPass *llvm::createMicroBlazeDelaySlotFiller() {
  return new MicroBlazeDelaySlotFiller();
}
