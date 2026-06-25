//===-- MicroBlazeSubtarget.cpp - MicroBlaze Subtarget Information --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeSubtarget.h"
#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "MicroBlazeTargetMachine.h"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "MicroBlazeGenSubtargetInfo.inc"

using namespace llvm;

// Call ParseSubtargetFeatures before any member that queries feature bits
// (TLInfo constructor reads hasBarrelShift() etc., so features must be set first).
static MicroBlazeSubtarget &
initSubtargetDependencies(StringRef CPU, StringRef FS,
                          MicroBlazeSubtarget &STI) {
  STI.ParseSubtargetFeatures(CPU, CPU, FS);
  return STI;
}

MicroBlazeSubtarget::MicroBlazeSubtarget(const Triple &TT, StringRef CPU,
                                         StringRef FS,
                                         const MicroBlazeTargetMachine &TM)
    : MicroBlazeGenSubtargetInfo(TT, CPU, /*TuneCPU=*/CPU, FS),
      InstrInfo(initSubtargetDependencies(CPU, FS, *this)),
      FrameLowering(*this), TLInfo(TM, *this) {}
