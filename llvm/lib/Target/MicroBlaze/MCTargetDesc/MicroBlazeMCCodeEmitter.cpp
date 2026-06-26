//===-- MicroBlazeMCCodeEmitter.cpp - Convert MicroBlaze code to bytes ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MicroBlaze instruction encoding (UG984 Ch.5):
//
//   Type A: [31:26]=opcode [25:21]=rD [20:16]=rA [15:11]=rB [10:0]=func
//   Type B: [31:26]=opcode [25:21]=rD [20:16]=rA [15:0]=imm16
//
// Immediates that do not fit in a signed 16-bit field require an IMM prefix
// instruction (opcode=0x2C) carrying the upper 16 bits.  The MCCodeEmitter
// emits the IMM prefix automatically when it detects a large or symbolic
// immediate operand in a Type B instruction.
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeFixupKinds.h"
#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "mccodeemitter"

using namespace llvm;

namespace {

class MicroBlazeMCCodeEmitter : public MCCodeEmitter {
  const MCInstrInfo &MCII;
  const MCRegisterInfo &MRI;
  MCContext &Ctx;

public:
  MicroBlazeMCCodeEmitter(const MCInstrInfo &MCII, const MCRegisterInfo &MRI,
                          MCContext &Ctx)
      : MCII(MCII), MRI(MRI), Ctx(Ctx) {}

  ~MicroBlazeMCCodeEmitter() override = default;

  // TableGen-generated.
  uint64_t getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

  // Return binary encoding of a single operand.
  unsigned getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  // Encode a memory operand: base reg | imm16 (for memri/memrr operands).
  unsigned getMemOpValue(const MCInst &MI, unsigned OpNo,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const;

  void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;

private:
  // Return true if the Type B instruction has an immediate operand that
  // requires an IMM prefix (value does not fit in sext16, or is symbolic).
  bool needsIMMPrefix(const MCInst &MI, SmallVectorImpl<MCFixup> &Fixups,
                      bool &IsPCRel) const;

  // Emit a 4-byte IMM prefix instruction carrying the high 16 bits of Val.
  // Adds a fixup if Expr is non-null (symbolic operand).
  void emitIMMPrefix(uint64_t CurSize, const MCExpr *Expr, int64_t Val,
                     bool IsPCRel, SmallVectorImpl<char> &CB,
                     SmallVectorImpl<MCFixup> &Fixups) const;
};

} // end anonymous namespace

// Return the hardware register number for a register operand.
// MicroBlaze registers R0-R31 have HW encodings 0-31 matching their numbers.
unsigned MicroBlazeMCCodeEmitter::getMachineOpValue(
    const MCInst &MI, const MCOperand &MO, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  if (MO.isReg())
    return MRI.getEncodingValue(MO.getReg());
  if (MO.isImm())
    return static_cast<unsigned>(MO.getImm()) & 0xFFFF;

  // Symbolic operand — record a fixup and return zero.
  assert(MO.isExpr() && "Expected register, immediate, or expression");
  // The fixup type and offset are refined in encodeInstruction; here we
  // conservatively use FIXUP_MICROBLAZE_32 as a placeholder.
  Fixups.push_back(MCFixup::create(0, MO.getExpr(),
                                   MCFixupKind(MicroBlaze::FIXUP_MICROBLAZE_32)));
  return 0;
}

// Encode a memri/memrr complex operand.
// memri: [20:16]=base, [15:0]=imm16.
// memrr: [20:16]=base, [15:11]=index.
unsigned MicroBlazeMCCodeEmitter::getMemOpValue(
    const MCInst &MI, unsigned OpNo, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  const MCOperand &Base = MI.getOperand(OpNo);
  const MCOperand &Off = MI.getOperand(OpNo + 1);
  unsigned BaseReg = MRI.getEncodingValue(Base.getReg());
  if (Off.isReg()) {
    // memrr: pack both registers into the combined 10-bit field.
    unsigned IdxReg = MRI.getEncodingValue(Off.getReg());
    return (BaseReg << 5) | IdxReg;
  }
  // memri: pack base + signed 16-bit offset.
  unsigned Offset = Off.isImm() ? (unsigned)Off.getImm() & 0xFFFF : 0;
  if (Off.isExpr())
    Fixups.push_back(MCFixup::create(0, Off.getExpr(),
                                     MCFixupKind(MicroBlaze::FIXUP_MICROBLAZE_32)));
  return (BaseReg << 16) | Offset;
}

// Determine if MI is a Type B instruction with an operand requiring an IMM
// prefix.  Sets IsPCRel to true for branch/call instructions.
bool MicroBlazeMCCodeEmitter::needsIMMPrefix(const MCInst &MI,
                                              SmallVectorImpl<MCFixup> &Fixups,
                                              bool &IsPCRel) const {
  const MCInstrDesc &Desc = MCII.get(MI.getOpcode());
  IsPCRel = false;

  // Only Type B instructions (those with an imm16 field) can have an IMM prefix.
  // We identify them by the presence of an OPERAND_IMMEDIATE or OPERAND_PCREL
  // operand in the instruction definition.
  for (unsigned i = 0, e = MI.getNumOperands(); i < e; ++i) {
    const MCOperandInfo &OpInfo = Desc.operands()[i];
    if (OpInfo.OperandType == MCOI::OPERAND_PCREL)
      IsPCRel = true;

    if (OpInfo.OperandType != MCOI::OPERAND_IMMEDIATE &&
        OpInfo.OperandType != MCOI::OPERAND_PCREL)
      continue;

    const MCOperand &MO = MI.getOperand(i);
    if (MO.isExpr())
      return true; // Symbolic — always needs IMM prefix for 32-bit address.
    if (MO.isImm()) {
      int64_t V = MO.getImm();
      if (!isInt<16>(V))
        return true; // Large constant — doesn't fit in imm16.
    }
  }
  return false;
}

// Emit a 4-byte IMM prefix instruction.
// IMM encoding: opcode=0x2C [31:26], rD=rA=0 [25:16], imm_hi16 [15:0].
void MicroBlazeMCCodeEmitter::emitIMMPrefix(
    uint64_t CurSize, const MCExpr *Expr, int64_t Val, bool IsPCRel,
    SmallVectorImpl<char> &CB, SmallVectorImpl<MCFixup> &Fixups) const {
  uint32_t IMMWord = 0xB0000000u; // opcode 0x2C | rD=0 | rA=0 | imm_hi16=0

  if (Expr) {
    // Symbolic operand: emit a relocation on the IMM word covering both the
    // IMM instruction and the following instruction (the 64-bit pair).
    // Check for PIC specifier expressions (GOT, PLT) first.
    MicroBlaze::Fixups FKind;
    if (const auto *SE = dyn_cast<MCSpecifierExpr>(Expr)) {
      unsigned Spec = SE->getSpecifier();
      if (Spec == ELF::R_MICROBLAZE_GOT_64)
        FKind = MicroBlaze::FIXUP_MICROBLAZE_GOT_64;
      else if (Spec == ELF::R_MICROBLAZE_PLT_64)
        FKind = MicroBlaze::FIXUP_MICROBLAZE_PLT_64;
      else
        FKind = IsPCRel ? MicroBlaze::FIXUP_MICROBLAZE_64_PCREL
                        : MicroBlaze::FIXUP_MICROBLAZE_64;
    } else {
      FKind = IsPCRel ? MicroBlaze::FIXUP_MICROBLAZE_64_PCREL
                      : MicroBlaze::FIXUP_MICROBLAZE_64;
    }
    Fixups.push_back(MCFixup::create(CurSize, Expr, MCFixupKind(FKind)));
  } else {
    // Large compile-time constant: encode the high 16 bits in the IMM word.
    IMMWord |= (static_cast<uint32_t>(Val) >> 16) & 0xFFFF;
  }

  support::endian::write<uint32_t>(CB, IMMWord, llvm::endianness::little);
}

void MicroBlazeMCCodeEmitter::encodeInstruction(
    const MCInst &MI, SmallVectorImpl<char> &CB,
    SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {

  // Gather fixups from sub-operand encoding.
  SmallVector<MCFixup, 4> InstrFixups;
  uint64_t Binary = getBinaryCodeForInstr(MI, InstrFixups, STI);

  // Check if a preceding IMM instruction is required.
  bool IsPCRel = false;
  const MCExpr *SymExpr = nullptr;
  int64_t ImmVal = 0;
  bool NeedsIMM = false;

  // IMM (opcode 0x2C, UG984 §5.2) carries the high 16 bits for the following
  // Type B instruction; it never takes its own IMM prefix.
  if ((Binary >> 26) != 0x2Cu) {
    const MCInstrDesc &Desc = MCII.get(MI.getOpcode());
    for (unsigned i = 0, e = MI.getNumOperands(); i < e; ++i) {
      const MCOperandInfo &OpInfo = Desc.operands()[i];
      if (OpInfo.OperandType == MCOI::OPERAND_PCREL)
        IsPCRel = true;
      if (OpInfo.OperandType != MCOI::OPERAND_IMMEDIATE &&
          OpInfo.OperandType != MCOI::OPERAND_PCREL)
        continue;
      const MCOperand &MO = MI.getOperand(i);
      if (MO.isExpr()) {
        SymExpr = MO.getExpr();
        NeedsIMM = true;
      } else if (MO.isImm()) {
        ImmVal = MO.getImm();
        if (!isInt<16>(ImmVal))
          NeedsIMM = true;
      }
    }
  }

  if (NeedsIMM) {
    // Emit IMM prefix; its fixup (if any) is at the current CB offset.
    emitIMMPrefix(CB.size(), SymExpr, ImmVal, IsPCRel, CB, Fixups);
    // Mask Binary's imm16 to the low 16 bits only.
    Binary = (Binary & 0xFFFF0000u) | (static_cast<uint32_t>(ImmVal) & 0xFFFF);
    // Symbolic fixups from the sub-operand encoder are no longer needed
    // (the IMM-prefix fixup covers the pair); drop them.
    InstrFixups.clear();
  }

  // Adjust fixup offsets to be relative to start of *this* instruction word.
  for (MCFixup &F : InstrFixups) {
    Fixups.push_back(
        MCFixup::create(CB.size() + F.getOffset(), F.getValue(), F.getKind()));
  }

  support::endian::write<uint32_t>(CB, static_cast<uint32_t>(Binary),
                                   llvm::endianness::little);
}

#include "MicroBlazeGenMCCodeEmitter.inc"

MCCodeEmitter *llvm::createMicroBlazeMCCodeEmitter(const MCInstrInfo &MCII,
                                                    MCContext &Ctx) {
  return new MicroBlazeMCCodeEmitter(MCII, *Ctx.getRegisterInfo(), Ctx);
}
