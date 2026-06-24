//===-- MicroBlazeFrameLowering.cpp - MicroBlaze Frame Lowering -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Stub — full implementation in Commit 6 (SelectionDAG lowering).
//===----------------------------------------------------------------------===//

#include "MicroBlazeFrameLowering.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

MicroBlazeFrameLowering::MicroBlazeFrameLowering(
    const MicroBlazeSubtarget &)
    : TargetFrameLowering(TargetFrameLowering::StackGrowsDown,
                          /*StackAlignment=*/Align(4),
                          /*LocalAreaOffset=*/0) {}

void MicroBlazeFrameLowering::emitPrologue(MachineFunction &,
                                           MachineBasicBlock &) const {
  report_fatal_error("MicroBlazeFrameLowering::emitPrologue not yet implemented");
}

void MicroBlazeFrameLowering::emitEpilogue(MachineFunction &,
                                           MachineBasicBlock &) const {
  report_fatal_error("MicroBlazeFrameLowering::emitEpilogue not yet implemented");
}

bool MicroBlazeFrameLowering::hasFPImpl(const MachineFunction &) const {
  return false;
}
