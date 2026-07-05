//===-- MicroBlazeMachineFunctionInfo.h - MicroBlaze Function Info
//-*-C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEMACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class MicroBlazeMachineFunctionInfo : public MachineFunctionInfo {
  // True once the link register (R15) has been spilled to the stack.
  bool HasSpilledLR = false;
  // Frame index of the LR spill slot (-1 = not yet allocated).
  int LRSpillSlot = -1;
  // Size of the outgoing argument area reserved in the stack frame.
  unsigned OutArgRegSize = 0;
  // Frame index of the vararg register save area (valid only in vararg
  // functions).
  int VarArgsFrameIndex = 0;
  // Number of bytes saved for incoming register varargs (0 if none).
  int VarArgsSaveSize = 0;
  // Interrupt/save-volatiles handlers: frame slots for the dedicated registers
  // R17/R18 (always saved) and the MSR (interrupt CC only).  -1 = unused.
  int R17SpillSlot = -1;
  int R18SpillSlot = -1;
  int MSRSpillSlot = -1;
  // Frame index of the R19 (frame pointer) save slot when hasFP is true.
  int FPSpillSlot = -1;

public:
  MicroBlazeMachineFunctionInfo() = default;
  explicit MicroBlazeMachineFunctionInfo(const Function &,
                                         const TargetSubtargetInfo *) {}

  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &MBBMap)
      const override {
    return DestMF.cloneInfo<MicroBlazeMachineFunctionInfo>(*this);
  }

  bool hasSpilledLR() const { return HasSpilledLR; }
  void setSpilledLR(bool S) { HasSpilledLR = S; }

  int getLRSpillSlot() const { return LRSpillSlot; }
  void setLRSpillSlot(int S) { LRSpillSlot = S; }

  unsigned getOutArgRegSize() const { return OutArgRegSize; }
  void setOutArgRegSize(unsigned S) { OutArgRegSize = S; }

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  void setVarArgsFrameIndex(int I) { VarArgsFrameIndex = I; }

  int getVarArgsSaveSize() const { return VarArgsSaveSize; }
  void setVarArgsSaveSize(int S) { VarArgsSaveSize = S; }

  int getR17SpillSlot() const { return R17SpillSlot; }
  void setR17SpillSlot(int S) { R17SpillSlot = S; }
  int getR18SpillSlot() const { return R18SpillSlot; }
  void setR18SpillSlot(int S) { R18SpillSlot = S; }
  int getMSRSpillSlot() const { return MSRSpillSlot; }
  void setMSRSpillSlot(int S) { MSRSpillSlot = S; }
  int getFPSpillSlot() const { return FPSpillSlot; }
  void setFPSpillSlot(int S) { FPSpillSlot = S; }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEMACHINEFUNCTIONINFO_H
