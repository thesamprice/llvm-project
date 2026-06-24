//===-- MicroBlazeRegisterInfo.cpp - MicroBlaze Register Information ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeRegisterInfo.h"
#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "MicroBlazeSubtarget.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/Support/ErrorHandling.h"

#define GET_REGINFO_TARGET_DESC
#include "MicroBlazeGenRegisterInfo.inc"

using namespace llvm;

// R15 is the link register — pass it as the return-address register so that
// DWARF unwind info correctly identifies it as the return column.
MicroBlazeRegisterInfo::MicroBlazeRegisterInfo()
    : MicroBlazeGenRegisterInfo(MicroBlaze::R15) {}

const uint16_t *
MicroBlazeRegisterInfo::getCalleeSavedRegs(const MachineFunction *) const {
  return CSR_SaveList;
}

const uint32_t *
MicroBlazeRegisterInfo::getCallPreservedMask(const MachineFunction &,
                                             CallingConv::ID) const {
  return CSR_RegMask;
}

BitVector
MicroBlazeRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());

  // R0  — hardwired zero (writes discarded by hardware).
  Reserved.set(MicroBlaze::R0);
  // R1  — stack pointer.
  Reserved.set(MicroBlaze::R1);
  // R2  — read-only small-data anchor (_SDA2_BASE_).
  Reserved.set(MicroBlaze::R2);
  // R13 — read-write small-data anchor (_SDA_BASE_).
  Reserved.set(MicroBlaze::R13);
  // R14 — interrupt return address (architectural, UG984 §2).
  Reserved.set(MicroBlaze::R14);
  // R15 — link register / return address for subroutine calls.
  Reserved.set(MicroBlaze::R15);
  // R16 — break return address (architectural, UG984 §2).
  Reserved.set(MicroBlaze::R16);
  // R17 — exception return address; hardware exceptions assumed always present.
  Reserved.set(MicroBlaze::R17);
  // R18 — assembler temporary, reserved by ABI.
  Reserved.set(MicroBlaze::R18);

  // R19 is the frame pointer; only reserved when actually in use.
  if (MF.getSubtarget<MicroBlazeSubtarget>().getFrameLowering()->hasFP(MF))
    Reserved.set(MicroBlaze::R19);

  return Reserved;
}

bool MicroBlazeRegisterInfo::requiresRegisterScavenging(
    const MachineFunction &) const {
  return true;
}

bool MicroBlazeRegisterInfo::eliminateFrameIndex(
    MachineBasicBlock::iterator II, int SPAdj, unsigned FIOperandNum,
    RegScavenger *RS) const {
  // Implemented in Commit 6 (frame lowering).
  report_fatal_error("MicroBlazeRegisterInfo::eliminateFrameIndex not yet implemented");
}

Register
MicroBlazeRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return MF.getSubtarget<MicroBlazeSubtarget>().getFrameLowering()->hasFP(MF)
             ? MicroBlaze::R19
             : MicroBlaze::R1;
}
