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

  bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                     MachineBasicBlock *&FBB,
                     SmallVectorImpl<MachineOperand> &Cond,
                     bool AllowModify = false) const override;

  unsigned removeBranch(MachineBasicBlock &MBB,
                        int *BytesRemoved = nullptr) const override;

  bool reverseBranchCondition(
      SmallVectorImpl<MachineOperand> &Cond) const override;

  unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                        MachineBasicBlock *FBB,
                        ArrayRef<MachineOperand> Cond,
                        const DebugLoc &DL,
                        int *BytesAdded = nullptr) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEINSTRINFO_H
