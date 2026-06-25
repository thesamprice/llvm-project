//===-- MicroBlazeDisassembler.h - Disassembler for MicroBlaze ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_DISASSEMBLER_MICROBLAZEDISASSEMBLER_H
#define LLVM_LIB_TARGET_MICROBLAZE_DISASSEMBLER_MICROBLAZEDISASSEMBLER_H

#include "llvm/MC/MCDisassembler/MCDisassembler.h"

namespace llvm {

class MCSubtargetInfo;
class MCContext;

class MicroBlazeDisassembler : public MCDisassembler {
public:
  MicroBlazeDisassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_DISASSEMBLER_MICROBLAZEDISASSEMBLER_H
