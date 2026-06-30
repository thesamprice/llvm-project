//===-- MicroBlazeISelLowering.h - MicroBlaze DAG Lowering ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Stub — full implementation in Commit 6 (SelectionDAG lowering).
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEISELLOWERING_H
#define LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEISELLOWERING_H

#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

class MicroBlazeSubtarget;
class MicroBlazeTargetMachine;

class MicroBlazeTargetLowering : public TargetLowering {
public:
  explicit MicroBlazeTargetLowering(const MicroBlazeTargetMachine &TM,
                                    const MicroBlazeSubtarget &STI);
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEISELLOWERING_H
