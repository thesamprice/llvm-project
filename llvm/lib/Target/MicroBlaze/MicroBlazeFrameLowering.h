//===-- MicroBlazeFrameLowering.h - MicroBlaze Frame Lowering ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Stub — full implementation in Commit 6 (SelectionDAG lowering).
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEFRAMELOWERING_H
#define LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEFRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {

class MicroBlazeSubtarget;

class MicroBlazeFrameLowering : public TargetFrameLowering {
public:
  explicit MicroBlazeFrameLowering(const MicroBlazeSubtarget &STI);

  void emitPrologue(MachineFunction &MF,
                    MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF,
                    MachineBasicBlock &MBB) const override;
  bool hasFPImpl(const MachineFunction &MF) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEFRAMELOWERING_H
