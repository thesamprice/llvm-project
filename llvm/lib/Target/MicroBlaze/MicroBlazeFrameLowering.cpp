//===-- MicroBlazeFrameLowering.cpp - MicroBlaze Frame Lowering -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MicroBlaze ILP32 stack frame layout (stack grows down):
//
//   High address (caller's frame)
//   ┌─────────────────────────────┐
//   │  Incoming arguments (>6)    │  ← fixed, caller-allocated
//   ├─────────────────────────────┤  ← old SP (R1) on entry
//   │  Link register (R15) save   │  ← SP_new + StackSize - 4
//   │  Callee-saved registers     │  ← PEI-allocated spill slots
//   │  Local variables / spills   │
//   └─────────────────────────────┘
//   Low address (our frame, new SP after prologue)
//
// Prologue emits:   addik r1, r1, -StackSize
//                   swi   r15, r1, (StackSize - 4)     [if non-leaf]
// Epilogue emits:   lwi   r15, r1, (StackSize - 4)     [if non-leaf]
//                   addik r1, r1, +StackSize
//
// Callee-saved registers (R22–R31) are saved/restored by
// spillCalleeSavedRegisters / restoreCalleeSavedRegisters using PEI-assigned
// frame indices; those spill slots are part of StackSize.
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeFrameLowering.h"
#include "MicroBlazeInstrInfo.h"
#include "MicroBlazeMachineFunctionInfo.h"
#include "MicroBlazeSubtarget.h"
#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterScavenging.h"

using namespace llvm;

MicroBlazeFrameLowering::MicroBlazeFrameLowering(
    const MicroBlazeSubtarget &STI)
    : TargetFrameLowering(TargetFrameLowering::StackGrowsDown, Align(8), 0),
      STI(STI) {}

bool MicroBlazeFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  return false;
}

//===----------------------------------------------------------------------===//
// determineCalleeSaves — reserve a stack slot for R15 when the function
// makes calls (otherwise R15 is overwritten by brald and the return address
// is lost).
//===----------------------------------------------------------------------===//

void MicroBlazeFrameLowering::determineCalleeSaves(MachineFunction &MF,
                                                   BitVector &SavedRegs,
                                                   RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);

  MachineFrameInfo &MFI = MF.getFrameInfo();
  auto *FuncInfo = MF.getInfo<MicroBlazeMachineFunctionInfo>();

  // If the function makes calls, R15 (link register) is clobbered by brald.
  // Reserve a 4-byte stack slot for it so emitPrologue/emitEpilogue can save
  // and restore it.
  if (MFI.adjustsStack()) {
    int LRSlot = MFI.CreateStackObject(4, Align(4), /*isSS=*/true);
    FuncInfo->setLRSpillSlot(LRSlot);
    FuncInfo->setSpilledLR(true);
  }
}

//===----------------------------------------------------------------------===//
// spillCalleeSavedRegisters / restoreCalleeSavedRegisters
//===----------------------------------------------------------------------===//

bool MicroBlazeFrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty())
    return true;
  MachineFunction &MF = *MBB.getParent();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();

  for (const CalleeSavedInfo &CS : CSI) {
    BuildMI(MBB, MI, DL, TII.get(MicroBlaze::SWI))
        .addReg(CS.getReg(), RegState::Kill)
        .addFrameIndex(CS.getFrameIdx())
        .addImm(0)
        .setMIFlag(MachineInstr::FrameSetup);
  }
  return true;
}

bool MicroBlazeFrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI,
    const TargetRegisterInfo *TRI) const {
  if (CSI.empty())
    return true;
  MachineFunction &MF = *MBB.getParent();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();

  for (const CalleeSavedInfo &CS : llvm::reverse(CSI)) {
    BuildMI(MBB, MI, DL, TII.get(MicroBlaze::LWI), CS.getReg())
        .addFrameIndex(CS.getFrameIdx())
        .addImm(0)
        .setMIFlag(MachineInstr::FrameDestroy);
  }
  return true;
}

//===----------------------------------------------------------------------===//
// Prologue
//===----------------------------------------------------------------------===//

void MicroBlazeFrameLowering::emitPrologue(MachineFunction &MF,
                                            MachineBasicBlock &MBB) const {
  MachineBasicBlock::iterator MBBI = MBB.begin();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  auto *MBlazeI = STI.getInstrInfo();
  auto *FuncInfo = MF.getInfo<MicroBlazeMachineFunctionInfo>();
  DebugLoc DL;

  uint64_t StackSize = MFI.getStackSize();
  if (StackSize == 0)
    return;

  if (!isInt<16>(-(int64_t)StackSize))
    report_fatal_error("MicroBlaze: stack frame too large for ADDIK");

  // addik r1, r1, -StackSize
  BuildMI(MBB, MBBI, DL, MBlazeI->get(MicroBlaze::ADDIK), MicroBlaze::R1)
      .addReg(MicroBlaze::R1)
      .addImm(-(int64_t)StackSize)
      .setMIFlag(MachineInstr::FrameSetup);

  // Save R15 (link register) if this function makes calls.
  if (FuncInfo->hasSpilledLR()) {
    int LRSlot = FuncInfo->getLRSpillSlot();
    // getObjectOffset returns offset relative to the fixed-object area top
    // (which is at the incoming SP). Add StackSize to get SP-relative offset.
    int64_t Offset = MFI.getObjectOffset(LRSlot) + (int64_t)StackSize;
    BuildMI(MBB, MBBI, DL, MBlazeI->get(MicroBlaze::SWI))
        .addReg(MicroBlaze::R15)
        .addReg(MicroBlaze::R1)
        .addImm(Offset)
        .setMIFlag(MachineInstr::FrameSetup);
  }
}

//===----------------------------------------------------------------------===//
// Epilogue
//===----------------------------------------------------------------------===//

void MicroBlazeFrameLowering::emitEpilogue(MachineFunction &MF,
                                            MachineBasicBlock &MBB) const {
  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  auto *MBlazeI = STI.getInstrInfo();
  auto *FuncInfo = MF.getInfo<MicroBlazeMachineFunctionInfo>();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();

  uint64_t StackSize = MFI.getStackSize();
  if (StackSize == 0)
    return;

  // Restore R15 (link register) if it was saved in the prologue.
  if (FuncInfo->hasSpilledLR()) {
    int LRSlot = FuncInfo->getLRSpillSlot();
    int64_t Offset = MFI.getObjectOffset(LRSlot) + (int64_t)StackSize;
    BuildMI(MBB, MBBI, DL, MBlazeI->get(MicroBlaze::LWI), MicroBlaze::R15)
        .addReg(MicroBlaze::R1)
        .addImm(Offset)
        .setMIFlag(MachineInstr::FrameDestroy);
  }

  // addik r1, r1, +StackSize
  BuildMI(MBB, MBBI, DL, MBlazeI->get(MicroBlaze::ADDIK), MicroBlaze::R1)
      .addReg(MicroBlaze::R1)
      .addImm((int64_t)StackSize)
      .setMIFlag(MachineInstr::FrameDestroy);
}

//===----------------------------------------------------------------------===//
// Call frame pseudo elimination
//===----------------------------------------------------------------------===//

MachineBasicBlock::iterator
MicroBlazeFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction & /*MF*/, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  return MBB.erase(I);
}
