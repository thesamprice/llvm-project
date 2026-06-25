//===-- MicroBlazeSubtarget.h - MicroBlaze Subtarget Info -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZESUBTARGET_H
#define LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZESUBTARGET_H

#include "MicroBlazeFrameLowering.h"
#include "MicroBlazeISelLowering.h"
#include "MicroBlazeInstrInfo.h"
#include "MicroBlazeRegisterInfo.h"
#include "llvm/CodeGen/LibcallLoweringInfo.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DataLayout.h"
#include <string>

#define GET_SUBTARGETINFO_HEADER
#include "MicroBlazeGenSubtargetInfo.inc"

namespace llvm {

class MicroBlazeTargetMachine;
class StringRef;

class MicroBlazeSubtarget : public MicroBlazeGenSubtargetInfo {
  // Optional ISA features (set by SubtargetFeature flags in MicroBlaze.td).
  bool HasBarrelShift = false;
  bool HasMultiplyHigh = false;
  bool HasPatternCompare = false;
  bool HasDivide = false;
  bool HasReorderInstr = false;
  bool HasHardFloat = false;
  bool HasFloatConvert = false;

  SelectionDAGTargetInfo TSI;
  MicroBlazeInstrInfo InstrInfo;
  MicroBlazeRegisterInfo RegInfo;
  MicroBlazeFrameLowering FrameLowering;
  MicroBlazeTargetLowering TLInfo;

public:
  MicroBlazeSubtarget(const Triple &TT, StringRef CPU, StringRef FS,
                      const MicroBlazeTargetMachine &TM);

  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);

  const MicroBlazeInstrInfo *getInstrInfo() const override {
    return &InstrInfo;
  }
  const MicroBlazeRegisterInfo *getRegisterInfo() const override {
    return &RegInfo;
  }
  const MicroBlazeFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const MicroBlazeTargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
  const SelectionDAGTargetInfo *getSelectionDAGInfo() const override {
    return &TSI;
  }

  bool hasBarrelShift() const { return HasBarrelShift; }
  bool hasMultiplyHigh() const { return HasMultiplyHigh; }
  bool hasPatternCompare() const { return HasPatternCompare; }
  bool hasDivide() const { return HasDivide; }
  bool hasReorderInstr() const { return HasReorderInstr; }
  bool hasHardFloat() const { return HasHardFloat; }
  bool hasFloatConvert() const { return HasFloatConvert; }

  void initLibcallLoweringInfo(LibcallLoweringInfo &Info) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZESUBTARGET_H
