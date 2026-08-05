//===-- MicroBlazeMCTargetDesc.cpp - MicroBlaze Target Descriptions -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeMCTargetDesc.h"
#include "MicroBlazeInstPrinter.h"
#include "MicroBlazeMCAsmInfo.h"
#include "TargetInfo/MicroBlazeTargetInfo.h"
#include "llvm/MC/MCInstrAnalysis.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

#define GET_INSTRINFO_MC_DESC
#define GET_INSTRINFO_MC_HELPERS
#include "MicroBlazeGenInstrInfo.inc"

#define GET_REGINFO_MC_DESC
#include "MicroBlazeGenRegisterInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "MicroBlazeGenSubtargetInfo.inc"

using namespace llvm;

static MCInstrInfo *createMicroBlazeMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitMicroBlazeMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createMicroBlazeMCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  // R15 is the return-address / link register.
  InitMicroBlazeMCRegisterInfo(X, MicroBlaze::R15);
  return X;
}

static MCSubtargetInfo *
createMicroBlazeMCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
  std::string CPUName = std::string(CPU);
  if (CPUName.empty())
    CPUName = "generic";
  // The Area scheduling model (C_AREA_OPTIMIZED=1) is selected feature-
  // orthogonally via +area-optimized, mapped to the "mb-area" tune-CPU.  This
  // mirrors MicroBlazeSubtarget so MC-level tools (llvm-mca, llvm-mc) agree.
  StringRef TuneCPU = CPUName;
  SmallVector<StringRef, 8> Features;
  FS.split(Features, ',', /*MaxSplit=*/-1, /*KeepEmpty=*/false);
  for (StringRef F : Features)
    if (F == "+area-optimized")
      TuneCPU = "mb-area";
  return createMicroBlazeMCSubtargetInfoImpl(TT, CPUName, TuneCPU, FS);
}

// Generic MCInstrAnalysis (uses the isCall/isReturn/isBranch flags from the
// .td) — required by analysis tools such as llvm-mca, which dereference it.
static MCInstrAnalysis *
createMicroBlazeMCInstrAnalysis(const MCInstrInfo *Info) {
  return new MCInstrAnalysis(Info);
}

static MCInstPrinter *createMicroBlazeMCInstPrinter(const Triple &T,
                                                    unsigned SyntaxVariant,
                                                    const MCAsmInfo &MAI,
                                                    const MCInstrInfo &MII,
                                                    const MCRegisterInfo &MRI) {
  return new MicroBlazeInstPrinter(MAI, MII, MRI);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMicroBlazeTargetMC() {
  Target &T = getTheMicroBlazeELTarget();

  RegisterMCAsmInfo<MicroBlazeMCAsmInfo> X(T);
  TargetRegistry::RegisterMCInstrInfo(T, createMicroBlazeMCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(T, createMicroBlazeMCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createMicroBlazeMCSubtargetInfo);
  TargetRegistry::RegisterMCInstrAnalysis(T, createMicroBlazeMCInstrAnalysis);
  TargetRegistry::RegisterMCInstPrinter(T, createMicroBlazeMCInstPrinter);
  TargetRegistry::RegisterMCCodeEmitter(T, createMicroBlazeMCCodeEmitter);
  TargetRegistry::RegisterMCAsmBackend(T, createMicroBlazeAsmBackend);
}
