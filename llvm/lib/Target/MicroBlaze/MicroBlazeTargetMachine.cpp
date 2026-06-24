//===-- MicroBlazeTargetMachine.cpp - MicroBlaze Target Machine -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeTargetMachine.h"
#include "MicroBlaze.h"
#include "MicroBlazeISelDAGToDAG.h"
#include "TargetInfo/MicroBlazeTargetInfo.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/Target/TargetOptions.h"
#include <memory>
#include <optional>

using namespace llvm;

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMicroBlazeTarget() {
  RegisterTargetMachine<MicroBlazeTargetMachine> X(getTheMicroBlazeELTarget());
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  return RM.value_or(Reloc::Static);
}

MicroBlazeTargetMachine::MicroBlazeTargetMachine(
    const Target &T, const Triple &TT, StringRef Cpu, StringRef FeatureString,
    const TargetOptions &Options, std::optional<Reloc::Model> RM,
    std::optional<CodeModel::Model> CodeModel, CodeGenOptLevel OptLevel,
    bool JIT)
    : CodeGenTargetMachineImpl(
          T, TT.computeDataLayout(), TT, Cpu, FeatureString, Options,
          getEffectiveRelocModel(RM),
          getEffectiveCodeModel(CodeModel, CodeModel::Small), OptLevel),
      TLOF(std::make_unique<TargetLoweringObjectFileELF>()),
      Subtarget(TT, std::string(Cpu), std::string(FeatureString), *this) {
  initAsmInfo();
}

namespace {

class MicroBlazePassConfig : public TargetPassConfig {
public:
  MicroBlazePassConfig(MicroBlazeTargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  MicroBlazeTargetMachine &getMicroBlazeTargetMachine() const {
    return getTM<MicroBlazeTargetMachine>();
  }

  bool addInstSelector() override {
    addPass(createMicroBlazeISelDag(getMicroBlazeTargetMachine(),
                                    getOptLevel()));
    return false;
  }

  void addPreEmitPass() override {
    addPass(createMicroBlazeDelaySlotFiller());
  }
};

} // namespace

TargetPassConfig *
MicroBlazeTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new MicroBlazePassConfig(*this, PM);
}
