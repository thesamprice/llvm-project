//===-- MicroBlazeMCTargetDesc.h - MicroBlaze Target Descriptions *- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MCTARGETDESC_MICROBLAZEMCTARGETDESC_H
#define LLVM_LIB_TARGET_MICROBLAZE_MCTARGETDESC_MICROBLAZEMCTARGETDESC_H

namespace llvm {

class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class Target;

MCCodeEmitter *createMicroBlazeMCCodeEmitter(const MCInstrInfo &MCII,
                                              MCContext &Ctx);

} // namespace llvm

// Symbolic names for MicroBlaze registers (R0–R31 + ABI aliases).
#define GET_REGINFO_ENUM
#include "MicroBlazeGenRegisterInfo.inc"

// MicroBlaze subtarget feature bit indices.
#define GET_SUBTARGETINFO_ENUM
#include "MicroBlazeGenSubtargetInfo.inc"

// MicroBlaze instruction opcode enum (MicroBlaze::ADDK, etc.).
#define GET_INSTRINFO_ENUM
#include "MicroBlazeGenInstrInfo.inc"

#endif // LLVM_LIB_TARGET_MICROBLAZE_MCTARGETDESC_MICROBLAZEMCTARGETDESC_H
