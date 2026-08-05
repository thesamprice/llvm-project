//===-- MicroBlaze.h - Top-level interface for MicroBlaze -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZE_H
#define LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZE_H

namespace llvm {

class FunctionPass;
class Function;
class MicroBlazeTargetMachine;
class PassRegistry;

FunctionPass *createMicroBlazeDelaySlotFiller();
void initializeMicroBlazeDelaySlotFillerPass(PassRegistry &);

// Interrupt-ABI predicates (cc73/cc74 or the interrupt-handler/save-volatiles
// function attributes).  Defined in MicroBlazeRegisterInfo.cpp.
bool isMicroBlazeInterruptHandler(const Function &F);
bool isMicroBlazeInterruptFunc(const Function &F);

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZE_H
