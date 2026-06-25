//===-- MicroBlazeInstrInfo.cpp - MicroBlaze Instruction Info -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeInstrInfo.h"
#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "MicroBlazeSubtarget.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "MicroBlazeGenInstrInfo.inc"

using namespace llvm;

MicroBlazeInstrInfo::MicroBlazeInstrInfo(const MicroBlazeSubtarget &STI)
    : MicroBlazeGenInstrInfo(STI, *STI.getRegisterInfo()), RI() {}
