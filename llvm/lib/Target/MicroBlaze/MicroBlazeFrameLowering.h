//===-- MicroBlazeFrameLowering.h - MicroBlaze Frame Lowering ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEFRAMELOWERING_H
#define LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEFRAMELOWERING_H

#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {

class MicroBlazeSubtarget;

class MicroBlazeFrameLowering : public TargetFrameLowering {
  const MicroBlazeSubtarget &STI;

public:
  explicit MicroBlazeFrameLowering(const MicroBlazeSubtarget &STI);

  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  bool hasFPImpl(const MachineFunction &MF) const override;
  // Always pre-allocate the max outgoing call argument area in the frame so
  // that callee-saves (R19, R15) land above [SP+0..SP+MaxCallFrameSize-1].
  // The default (!hasFP) returns false at O0 where FP is always on, causing
  // eliminateCallFramePseudoInstr to erase ADJCALLSTACKDOWN/UP without
  // adjusting SP — leaving callee-saves aliased with outgoing call arg slots.
  bool hasReservedCallFrame(const MachineFunction &MF) const override {
    return true;
  }
  MachineBasicBlock::iterator
  eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I) const override;

  void determineCalleeSaves(MachineFunction &MF, BitVector &SavedRegs,
                            RegScavenger *RS = nullptr) const override;

  bool spillCalleeSavedRegisters(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MI,
                                 ArrayRef<CalleeSavedInfo> CSI,
                                 const TargetRegisterInfo *TRI) const override;

  bool
  restoreCalleeSavedRegisters(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MI,
                              MutableArrayRef<CalleeSavedInfo> CSI,
                              const TargetRegisterInfo *TRI) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEFRAMELOWERING_H
