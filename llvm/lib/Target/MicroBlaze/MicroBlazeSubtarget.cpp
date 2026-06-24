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

MicroBlazeSubtarget::MicroBlazeSubtarget(const Triple &TT, StringRef CPU,
                                         StringRef FS,
                                         const MicroBlazeTargetMachine &TM)
    : MicroBlazeGenSubtargetInfo(TT, CPU, /*TuneCPU=*/CPU, FS),
      FrameLowering(*this) {
  ParseSubtargetFeatures(CPU, CPU, FS);
}
