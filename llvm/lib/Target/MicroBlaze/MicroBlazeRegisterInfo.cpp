//===-- MicroBlazeRegisterInfo.cpp - MicroBlaze Register Information ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeRegisterInfo.h"
#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "MicroBlaze.h"
#include "MicroBlazeSubtarget.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/ErrorHandling.h"

#define GET_REGINFO_TARGET_DESC
#include "MicroBlazeGenRegisterInfo.inc"

using namespace llvm;

// R15 is the link register — pass it as the return-address register so that
// DWARF unwind info correctly identifies it as the return column.
MicroBlazeRegisterInfo::MicroBlazeRegisterInfo()
    : MicroBlazeGenRegisterInfo(MicroBlaze::R15) {}

// interrupt_handler / save_volatiles functions are identified by either the
// cc73/cc74 calling convention (IR-level) or the Clang-emitted function
// attribute (so call sites stay normal — see clang Targets/MicroBlaze.cpp).
// isMicroBlazeInterruptFunc: either kind (preserve volatiles + R17/R18).
// isMicroBlazeInterruptHandler: true interrupt only (also MSR + rtid).
bool llvm::isMicroBlazeInterruptHandler(const Function &F) {
  return F.getCallingConv() == CallingConv::MICROBLAZE_INTR ||
         F.hasFnAttribute("interrupt-handler");
}
bool llvm::isMicroBlazeInterruptFunc(const Function &F) {
  return isMicroBlazeInterruptHandler(F) ||
         F.getCallingConv() == CallingConv::MICROBLAZE_SVOL ||
         F.hasFnAttribute("save-volatiles");
}

const uint16_t *
MicroBlazeRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  // Interrupt / save-volatiles handlers must also preserve the volatile
  // registers (R3-R12) they clobber; PEI spills the used subset.
  if (MF && isMicroBlazeInterruptFunc(MF->getFunction()))
    return CSR_Interrupt_SaveList;
  return CSR_SaveList;
}

const uint32_t *
MicroBlazeRegisterInfo::getCallPreservedMask(const MachineFunction &,
                                             CallingConv::ID CC) const {
  // Keyed on the callee's CC only: a normal-CC call to a save_volatiles
  // function uses the default mask (the caller does not rely on the extra
  // preservation).
  if (CC == CallingConv::MICROBLAZE_INTR || CC == CallingConv::MICROBLAZE_SVOL)
    return CSR_Interrupt_RegMask;
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
  // R20 — GOT base pointer in PIC mode; reserved unconditionally (per GCC).
  Reserved.set(MicroBlaze::R20);
  // R21 — reserved by GCC alongside R20 for PIC/SDA anchoring.
  Reserved.set(MicroBlaze::R21);

  // R19 — only reserved when the frame pointer is actually in use.
  if (MF.getSubtarget<MicroBlazeSubtarget>().getFrameLowering()->hasFP(MF))
    Reserved.set(MicroBlaze::R19);

  return Reserved;
}

bool MicroBlazeRegisterInfo::requiresRegisterScavenging(
    const MachineFunction &) const {
  return true;
}

bool MicroBlazeRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                                 int SPAdj,
                                                 unsigned FIOperandNum,
                                                 RegScavenger *RS) const {
  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();
  const MachineFrameInfo &MFI = MF.getFrameInfo();

  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  // Offset from the stack pointer to the start of this frame object.
  // MachineFrameInfo::getObjectOffset returns offset relative to the
  // bottom of the fixed-object area; add stack size to get SP-relative.
  int64_t Offset = MFI.getObjectOffset(FrameIndex) + MFI.getStackSize() +
                   MI.getOperand(FIOperandNum + 1).getImm() + SPAdj;

  // Leave the full Offset in the instruction's immediate field even when it
  // exceeds 16 bits.  The MCCodeEmitter detects the overflow and automatically
  // emits an IMM prefix during encoding — keeping a single MachineInstr rather
  // than an IMM+target pair prevents the post-RA instruction scheduler from
  // reordering the pair and corrupting the IMM latch.

  // Replace the FrameIndex operand with R1 (stack pointer).
  MI.getOperand(FIOperandNum).ChangeToRegister(MicroBlaze::R1, /*isDef=*/false);
  MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset);
  return false;
}

Register
MicroBlazeRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return MF.getSubtarget<MicroBlazeSubtarget>().getFrameLowering()->hasFP(MF)
             ? MicroBlaze::R19
             : MicroBlaze::R1;
}
