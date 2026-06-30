//===-- MicroBlazeISelLowering.cpp - MicroBlaze DAG Lowering --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Stub — full implementation in Commit 6 (SelectionDAG lowering).
//===----------------------------------------------------------------------===//

#include "MicroBlazeISelLowering.h"
#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "MicroBlazeSubtarget.h"
#include "MicroBlazeTargetMachine.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"

using namespace llvm;

// Pull in the generated CC_MicroBlaze / RetCC_MicroBlaze functions.
// Must follow "using namespace llvm" — the generated code uses unqualified names.
#include "MicroBlazeGenCallingConv.inc"

MicroBlazeTargetLowering::MicroBlazeTargetLowering(
    const MicroBlazeTargetMachine &TM, const MicroBlazeSubtarget &STI)
    : TargetLowering(TM, STI) {}
