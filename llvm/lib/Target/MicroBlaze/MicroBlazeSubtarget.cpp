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

// Scheduling-model selection is bound to the TuneCPU.  The Area pipeline
// (C_AREA_OPTIMIZED=1) is requested feature-orthogonally via +area-optimized,
// so map that feature to the internal "mb-area" tune-CPU (which carries
// MicroBlazeAreaModel) before the base constructor picks the model.  The real
// CPU and the rest of the -mattr set are unaffected.
static bool hasAreaOptimizedFeature(StringRef FS) {
  SmallVector<StringRef, 8> Features;
  FS.split(Features, ',', /*MaxSplit=*/-1, /*KeepEmpty=*/false);
  for (StringRef F : Features)
    if (F == "+area-optimized")
      return true;
  return false;
}

static StringRef computeTuneCPU(StringRef CPU, StringRef FS) {
  if (hasAreaOptimizedFeature(FS))
    return "mb-area";
  return CPU.empty() ? "generic" : CPU;
}

// Call ParseSubtargetFeatures before any member that queries feature bits
// (TLInfo constructor reads hasBarrelShift() etc., so features must be set
// first). Default to "generic" when no CPU is explicitly specified so that the
// features listed for the generic ProcessorModel in MicroBlaze.td (e.g.
// +pattern-compare) are applied.
static MicroBlazeSubtarget &
initSubtargetDependencies(StringRef CPU, StringRef FS,
                          MicroBlazeSubtarget &STI) {
  StringRef EffectiveCPU = CPU.empty() ? "generic" : CPU;
  STI.ParseSubtargetFeatures(EffectiveCPU, computeTuneCPU(CPU, FS), FS);
  return STI;
}

MicroBlazeSubtarget::MicroBlazeSubtarget(const Triple &TT, StringRef CPU,
                                         StringRef FS,
                                         const MicroBlazeTargetMachine &TM)
    : MicroBlazeGenSubtargetInfo(TT, CPU, computeTuneCPU(CPU, FS), FS),
      InstrInfo(initSubtargetDependencies(CPU, FS, *this)),
      FrameLowering(*this), TLInfo(TM, *this) {}

// Register software-ABI libcalls that MicroBlaze requires but that are not
// auto-enabled by LLVM's LegacyDefaultSystemLibrary predicate.
void MicroBlazeSubtarget::initLibcallLoweringInfo(
    LibcallLoweringInfo &Info) const {
  const struct {
    RTLIB::Libcall Op;
    RTLIB::LibcallImpl Impl;
  } Calls[] = {
      // i32 arithmetic (no hardware divide; mul uses MUL instruction).
      {RTLIB::SDIV_I32, RTLIB::impl___divsi3},
      {RTLIB::UDIV_I32, RTLIB::impl___udivsi3},
      {RTLIB::SREM_I32, RTLIB::impl___modsi3},
      {RTLIB::UREM_I32, RTLIB::impl___umodsi3},
      {RTLIB::MUL_I32, RTLIB::impl___mulsi3},
      {RTLIB::SHL_I32, RTLIB::impl___ashlsi3},
      {RTLIB::SRL_I32, RTLIB::impl___lshrsi3},
      {RTLIB::SRA_I32, RTLIB::impl___ashrsi3},
      // i64 arithmetic: LLVM expands all 64-bit ops via libcalls on this 32-bit
      // target.
      {RTLIB::SDIV_I64, RTLIB::impl___divdi3},
      {RTLIB::UDIV_I64, RTLIB::impl___udivdi3},
      {RTLIB::SREM_I64, RTLIB::impl___moddi3},
      {RTLIB::UREM_I64, RTLIB::impl___umoddi3},
      {RTLIB::MUL_I64, RTLIB::impl___muldi3},
      {RTLIB::SHL_I64, RTLIB::impl___ashldi3},
      {RTLIB::SRL_I64, RTLIB::impl___lshrdi3},
      {RTLIB::SRA_I64, RTLIB::impl___ashrdi3},
      // Memory intrinsics: getMemcpy/getMemset/getMemmove use
      // SelectionDAG::Libcalls
      // (the analysis-pass copy), not TargetLowering::Libcalls. Both copies are
      // populated via initLibcallLoweringInfo, so registrations here are
      // required.
      {RTLIB::MEMCPY, RTLIB::impl_memcpy},
      {RTLIB::MEMMOVE, RTLIB::impl_memmove},
      {RTLIB::MEMSET, RTLIB::impl_memset},
  };
  for (const auto &LC : Calls)
    Info.setLibcallImpl(LC.Op, LC.Impl);
}
