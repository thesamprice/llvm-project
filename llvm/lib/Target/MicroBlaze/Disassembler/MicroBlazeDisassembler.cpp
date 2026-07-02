//===-- MicroBlazeDisassembler.cpp - Disassembler for MicroBlaze ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MicroBlaze disassembler. Reads 4 bytes (LE), reconstructs the 32-bit
// instruction word (big-endian bit numbering, UG984 Ch.5), and delegates to
// the TableGen-generated decoder table.
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeDisassembler.h"
#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "TargetInfo/MicroBlazeTargetInfo.h"
#include "llvm/MC/MCDecoder.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/MathExtras.h"

#define DEBUG_TYPE "microblaze-disassembler"

using namespace llvm;
using namespace llvm::MCD;

typedef MCDisassembler::DecodeStatus DecodeStatus;

// ===----------------------------------------------------------------------===
// Register decoder
// ===----------------------------------------------------------------------===

// MicroBlaze R0-R31 map hardware encoding 0-31 directly to register IDs.
static const unsigned GPRDecoderTable[] = {
  MicroBlaze::R0,  MicroBlaze::R1,  MicroBlaze::R2,  MicroBlaze::R3,
  MicroBlaze::R4,  MicroBlaze::R5,  MicroBlaze::R6,  MicroBlaze::R7,
  MicroBlaze::R8,  MicroBlaze::R9,  MicroBlaze::R10, MicroBlaze::R11,
  MicroBlaze::R12, MicroBlaze::R13, MicroBlaze::R14, MicroBlaze::R15,
  MicroBlaze::R16, MicroBlaze::R17, MicroBlaze::R18, MicroBlaze::R19,
  MicroBlaze::R20, MicroBlaze::R21, MicroBlaze::R22, MicroBlaze::R23,
  MicroBlaze::R24, MicroBlaze::R25, MicroBlaze::R26, MicroBlaze::R27,
  MicroBlaze::R28, MicroBlaze::R29, MicroBlaze::R30, MicroBlaze::R31
};

static DecodeStatus DecodeGPRRegisterClass(MCInst &Inst, unsigned RegNo,
                                           uint64_t /*Address*/,
                                           const MCDisassembler * /*Decoder*/) {
  if (RegNo > 31)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(GPRDecoderTable[RegNo]));
  return MCDisassembler::Success;
}

// FPR and GPR share the same physical registers; decoding is identical.
static DecodeStatus DecodeFPRRegisterClass(MCInst &Inst, unsigned RegNo,
                                           uint64_t /*Address*/,
                                           const MCDisassembler * /*Decoder*/) {
  if (RegNo > 31)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(GPRDecoderTable[RegNo]));
  return MCDisassembler::Success;
}

// Decode a memri complex operand: {base[20:16], offset[15:0]} from Type B.
// Adds base register then signed 16-bit offset.
static DecodeStatus decodeMemRIOperand(MCInst &Inst, unsigned Val,
                                       uint64_t /*Address*/,
                                       const MCDisassembler * /*Decoder*/) {
  unsigned Base = (Val >> 16) & 0x1F;
  unsigned Offset = Val & 0xFFFF;
  if (Base > 31)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(GPRDecoderTable[Base]));
  Inst.addOperand(MCOperand::createImm(SignExtend32<16>(Offset)));
  return MCDisassembler::Success;
}

// Decode a memrr complex operand: {base[9:5], index[4:0]} from Type A.
// Adds base register then index register.
static DecodeStatus decodeMemRROperand(MCInst &Inst, unsigned Val,
                                       uint64_t /*Address*/,
                                       const MCDisassembler * /*Decoder*/) {
  unsigned Base = (Val >> 5) & 0x1F;
  unsigned Idx  = Val & 0x1F;
  if (Base > 31 || Idx > 31)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(GPRDecoderTable[Base]));
  Inst.addOperand(MCOperand::createReg(GPRDecoderTable[Idx]));
  return MCDisassembler::Success;
}

// RFSL port registers rfsl0-rfsl15. Hardware encoding is the 4-bit port index
// placed in instruction bits[7:4] for static FSL get/put (opcode 0x1B).
static const unsigned RFSLDecoderTable[] = {
  MicroBlaze::rfsl0,  MicroBlaze::rfsl1,  MicroBlaze::rfsl2,  MicroBlaze::rfsl3,
  MicroBlaze::rfsl4,  MicroBlaze::rfsl5,  MicroBlaze::rfsl6,  MicroBlaze::rfsl7,
  MicroBlaze::rfsl8,  MicroBlaze::rfsl9,  MicroBlaze::rfsl10, MicroBlaze::rfsl11,
  MicroBlaze::rfsl12, MicroBlaze::rfsl13, MicroBlaze::rfsl14, MicroBlaze::rfsl15
};

static DecodeStatus DecodeRFSLRegisterClass(MCInst &Inst, unsigned RegNo,
                                            uint64_t /*Address*/,
                                            const MCDisassembler * /*Decoder*/) {
  if (RegNo > 15)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(RFSLDecoderTable[RegNo]));
  return MCDisassembler::Success;
}

#include "MicroBlazeGenDisassemblerTables.inc"

// ===----------------------------------------------------------------------===
// Instruction reading
// ===----------------------------------------------------------------------===

// MicroBlaze EL: instruction bytes are stored little-endian.
// Reconstruct the 32-bit word with the standard bit ordering used by UG984.
static DecodeStatus readInstruction32(ArrayRef<uint8_t> Bytes, uint64_t &Size,
                                      uint32_t &Insn) {
  if (Bytes.size() < 4) {
    Size = 0;
    return MCDisassembler::Fail;
  }
  Insn = (static_cast<uint32_t>(Bytes[3]) << 24) |
         (static_cast<uint32_t>(Bytes[2]) << 16) |
         (static_cast<uint32_t>(Bytes[1]) <<  8) |
         (static_cast<uint32_t>(Bytes[0]) <<  0);
  return MCDisassembler::Success;
}

// ===----------------------------------------------------------------------===
// Main disassembly entry point
// ===----------------------------------------------------------------------===

DecodeStatus MicroBlazeDisassembler::getInstruction(MCInst &Instr,
                                                     uint64_t &Size,
                                                     ArrayRef<uint8_t> Bytes,
                                                     uint64_t Address,
                                                     raw_ostream & /*CStream*/) const {
  uint32_t Insn;
  DecodeStatus Result = readInstruction32(Bytes, Size, Insn);
  if (Result == MCDisassembler::Fail)
    return MCDisassembler::Fail;

  Result = decodeInstruction(DecoderTable32, Instr, Insn, Address,
                             this, STI);
  if (Result != MCDisassembler::Fail) {
    Size = 4;
    return Result;
  }
  return MCDisassembler::Fail;
}

// ===----------------------------------------------------------------------===
// Registration
// ===----------------------------------------------------------------------===

static MCDisassembler *createMicroBlazeDisassembler(const Target & /*T*/,
                                                     const MCSubtargetInfo &STI,
                                                     MCContext &Ctx) {
  return new MicroBlazeDisassembler(STI, Ctx);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMicroBlazeDisassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheMicroBlazeELTarget(),
                                         createMicroBlazeDisassembler);
}
