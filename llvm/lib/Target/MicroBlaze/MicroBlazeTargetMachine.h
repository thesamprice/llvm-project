//===-- MicroBlazeTargetMachine.h - MicroBlaze TargetMachine ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZETARGETMACHINE_H
#define LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZETARGETMACHINE_H

#include "MicroBlazeMachineFunctionInfo.h"
#include "MicroBlazeSubtarget.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include <memory>
#include <optional>

namespace llvm {

class MicroBlazeTargetMachine : public CodeGenTargetMachineImpl {
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  MicroBlazeSubtarget Subtarget;
  mutable StringMap<std::unique_ptr<MicroBlazeSubtarget>> SubtargetMap;

public:
  MicroBlazeTargetMachine(const Target &TheTarget, const Triple &TargetTriple,
                          StringRef Cpu, StringRef FeatureString,
                          const TargetOptions &Options,
                          std::optional<Reloc::Model> RM,
                          std::optional<CodeModel::Model> CodeModel,
                          CodeGenOptLevel OptLevel, bool JIT);

  ~MicroBlazeTargetMachine() override = default;

  const MicroBlazeSubtarget *
  getSubtargetImpl(const Function &F) const override;

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }

  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

  MachineFunctionInfo *
  createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
                            const TargetSubtargetInfo *STI) const override {
    return MicroBlazeMachineFunctionInfo::create<MicroBlazeMachineFunctionInfo>(
        Allocator, F, STI);
  }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZETARGETMACHINE_H
