//===-- MicroBlazeTargetMachine.h - MicroBlaze TargetMachine ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZETARGETMACHINE_H
#define LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZETARGETMACHINE_H

#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include <optional>

namespace llvm {

class MicroBlazeTargetMachine : public CodeGenTargetMachineImpl {
public:
  MicroBlazeTargetMachine(const Target &TheTarget, const Triple &TargetTriple,
                          StringRef Cpu, StringRef FeatureString,
                          const TargetOptions &Options,
                          std::optional<Reloc::Model> RM,
                          std::optional<CodeModel::Model> CodeModel,
                          CodeGenOptLevel OptLevel, bool JIT);

  ~MicroBlazeTargetMachine() override = default;

  const TargetSubtargetInfo *
  getSubtargetImpl(const Function &) const override {
    return nullptr;
  }

  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZETARGETMACHINE_H
