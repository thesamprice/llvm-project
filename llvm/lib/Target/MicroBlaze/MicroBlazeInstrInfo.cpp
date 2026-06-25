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

#define GET_INSTRINFO_CTOR_DTOR
#include "MicroBlazeGenInstrInfo.inc"

using namespace llvm;

MicroBlazeInstrInfo::MicroBlazeInstrInfo(const MicroBlazeSubtarget &STI)
    : MicroBlazeGenInstrInfo(STI, *STI.getRegisterInfo()), RI() {}

// Tail-merger calls this only for unconditional branches (Cond is always empty
// because analyzeBranch returned true for all conditional cases).  Emit BRI
// and let the delay-slot filler add the NOP.
unsigned MicroBlazeInstrInfo::insertBranch(MachineBasicBlock &MBB,
                                            MachineBasicBlock *TBB,
                                            MachineBasicBlock *FBB,
                                            ArrayRef<MachineOperand> Cond,
                                            const DebugLoc &DL,
                                            int *BytesAdded) const {
  assert(TBB && !FBB && Cond.empty() && "Only unconditional branch supported");
  BuildMI(&MBB, DL, get(MicroBlaze::BRI)).addMBB(TBB);
  if (BytesAdded)
    *BytesAdded = 4;
  return 1;
}
