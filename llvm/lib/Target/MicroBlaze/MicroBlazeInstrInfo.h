//===-- MicroBlazeInstrInfo.h - MicroBlaze Instruction Info -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEINSTRINFO_H
#define LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEINSTRINFO_H

#include "MicroBlazeRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "MicroBlazeGenInstrInfo.inc"

namespace llvm {

class MicroBlazeSubtarget;

class MicroBlazeInstrInfo : public MicroBlazeGenInstrInfo {
  const MicroBlazeRegisterInfo RI;

public:
  explicit MicroBlazeInstrInfo(const MicroBlazeSubtarget &STI);

  const MicroBlazeRegisterInfo &getRegisterInfo() const { return RI; }

  // Return true (cannot analyze) to prevent the branch folder from
  // tail-merging or removing conditional branches around delay slots.
  bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                     MachineBasicBlock *&FBB,
                     SmallVectorImpl<MachineOperand> &Cond,
                     bool AllowModify = false) const override {
    return true;
  }

  // The tail-merger calls insertBranch to redirect merged tails via an
  // unconditional branch.  The delay-slot filler (which runs later) will
  // fill the one delay slot with a NOP if needed.
  unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                        MachineBasicBlock *FBB,
                        ArrayRef<MachineOperand> Cond,
                        const DebugLoc &DL,
                        int *BytesAdded = nullptr) const override;

  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                   const DebugLoc &DL, Register DstReg, Register SrcReg,
                   bool KillSrc, bool RenamableDst = false,
                   bool RenamableSrc = false) const override;

  void storeRegToStackSlot(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MBBI, Register SrcReg,
                           bool IsKill, int FrameIndex,
                           const TargetRegisterClass *RC, Register VReg,
                           MachineInstr::MIFlag Flags =
                               MachineInstr::NoFlags) const override;

  void loadRegFromStackSlot(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MBBI, Register DstReg,
                            int FrameIndex, const TargetRegisterClass *RC,
                            Register VReg, unsigned SubReg = 0,
                            MachineInstr::MIFlag Flags =
                                MachineInstr::NoFlags) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEINSTRINFO_H
