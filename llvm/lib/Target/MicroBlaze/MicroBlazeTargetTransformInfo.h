//===-- MicroBlazeTargetTransformInfo.h - MicroBlaze TTI --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZETARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZETARGETTRANSFORMINFO_H

#include "MicroBlazeSubtarget.h"
#include "MicroBlazeTargetMachine.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/BasicTTIImpl.h"

namespace llvm {

class MicroBlazeTTIImpl final
    : public BasicTTIImplBase<MicroBlazeTTIImpl> {
  using BaseT = BasicTTIImplBase<MicroBlazeTTIImpl>;
  friend BaseT;

  const MicroBlazeSubtarget *ST;
  const MicroBlazeTargetLowering *TLI;

  const MicroBlazeSubtarget *getST() const { return ST; }
  const MicroBlazeTargetLowering *getTLI() const { return TLI; }

public:
  explicit MicroBlazeTTIImpl(const MicroBlazeTargetMachine *TM,
                              const Function &F)
      : BaseT(TM, F.getDataLayout()),
        ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}

  // isLegalICmpImmediate: delegate to TLI (default: true for all immediates).
  // The LoopStrengthReduce cost model already adds +1 instruction for
  // non-zero-end ICmpZero formulas (via canMacroFuseCmp()=false), which
  // is enough to prefer countdown IVs when the loop variable is a pure
  // counter.  Returning false for non-zero would force countdown on ALL
  // loops — including array-indexed ones — which degrades code quality.
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZETARGETTRANSFORMINFO_H
