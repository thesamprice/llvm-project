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
#include "llvm/CodeGen/MachineInstrBuilder.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "MicroBlazeGenInstrInfo.inc"

using namespace llvm;

MicroBlazeInstrInfo::MicroBlazeInstrInfo(const MicroBlazeSubtarget &STI)
    : MicroBlazeGenInstrInfo(STI, *STI.getRegisterInfo(),
                             MicroBlaze::ADJCALLSTACKDOWN,
                             MicroBlaze::ADJCALLSTACKUP),
      RI() {}

void MicroBlazeInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator I,
                                       const DebugLoc &DL, Register DstReg,
                                       Register SrcReg, bool KillSrc,
                                       bool RenamableDst, bool RenamableSrc) const {
  // addk DstReg, SrcReg, R0  (= SrcReg + 0 = SrcReg)
  BuildMI(MBB, I, DL, get(MicroBlaze::ADDK), DstReg)
      .addReg(SrcReg, getKillRegState(KillSrc))
      .addReg(MicroBlaze::R0);
}

void MicroBlazeInstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, Register SrcReg,
    bool IsKill, int FrameIndex, const TargetRegisterClass * /*RC*/,
    Register /*VReg*/, MachineInstr::MIFlag Flags) const {
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();
  BuildMI(MBB, MBBI, DL, get(MicroBlaze::SWI))
      .addReg(SrcReg, getKillRegState(IsKill))
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .setMIFlag(Flags);
}

void MicroBlazeInstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, Register DstReg,
    int FrameIndex, const TargetRegisterClass * /*RC*/, Register /*VReg*/,
    unsigned /*SubReg*/, MachineInstr::MIFlag Flags) const {
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();
  BuildMI(MBB, MBBI, DL, get(MicroBlaze::LWI), DstReg)
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .setMIFlag(Flags);
}
