//===-- MicroBlazeMCTargetDesc.cpp - MicroBlaze Target Descriptions -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeMCTargetDesc.h"
#include "MicroBlazeMCAsmInfo.h"
#include "TargetInfo/MicroBlazeTargetInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

#define GET_SUBTARGETINFO_MC_DESC
#include "MicroBlazeGenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "MicroBlazeGenRegisterInfo.inc"

using namespace llvm;

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMicroBlazeTargetMC() {
  RegisterMCAsmInfo<MicroBlazeMCAsmInfo> X(getTheMicroBlazeELTarget());
}
