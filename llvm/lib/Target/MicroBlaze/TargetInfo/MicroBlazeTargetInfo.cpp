//===-- MicroBlazeTargetInfo.cpp - MicroBlaze Target Implementation -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

Target &llvm::getTheMicroBlazeELTarget() {
  static Target TheMicroBlazeELTarget;
  return TheMicroBlazeELTarget;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMicroBlazeTargetInfo() {
  RegisterTarget<Triple::microblazeel, /*HasJIT=*/false> X(
      getTheMicroBlazeELTarget(), "microblazeel", "MicroBlaze", "MicroBlaze");
}
