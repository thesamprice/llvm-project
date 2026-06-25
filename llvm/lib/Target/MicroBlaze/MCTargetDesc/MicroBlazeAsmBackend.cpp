//===-- MicroBlazeAsmBackend.cpp - MicroBlaze Assembler Backend -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeFixupKinds.h"
#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {

class MicroBlazeAsmBackend : public MCAsmBackend {
public:
  MicroBlazeAsmBackend()
      : MCAsmBackend(llvm::endianness::little) {}

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override;

  void applyFixup(const MCFragment &F, const MCFixup &Fixup,
                  const MCValue &Target, uint8_t *Data, uint64_t Value,
                  bool IsResolved) override;

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override;

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override;
};

} // end anonymous namespace

MCFixupKindInfo
MicroBlazeAsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  // Table must be in the same order as MicroBlaze::Fixups enum.
  // {Name, BitOffset, BitSize, Flags}
  // BitOffset is from the LSB of the fixup container (little-endian).
  static const MCFixupKindInfo Infos[MicroBlaze::NumTargetFixupKinds] = {
    // FIXUP_MICROBLAZE_NONE
    { "FIXUP_MICROBLAZE_NONE",    0, 0,  0 },
    // FIXUP_MICROBLAZE_32 — absolute 32-bit data word
    { "FIXUP_MICROBLAZE_32",      0, 32, 0 },
    // FIXUP_MICROBLAZE_32_PCREL — 16-bit PC-relative in instruction imm field
    { "FIXUP_MICROBLAZE_32_PCREL",0, 16, 0 },
    // FIXUP_MICROBLAZE_64 — two-word absolute (IMM + Type-B), on IMM word
    { "FIXUP_MICROBLAZE_64",      0, 32, 0 },
    // FIXUP_MICROBLAZE_64_PCREL — two-word PC-rel (IMM + branch), on IMM word
    { "FIXUP_MICROBLAZE_64_PCREL",0, 32, 0 },
  };

  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);

  assert(unsigned(Kind - FirstTargetFixupKind) < MicroBlaze::NumTargetFixupKinds
         && "Invalid fixup kind!");
  return Infos[Kind - FirstTargetFixupKind];
}

void MicroBlazeAsmBackend::applyFixup(const MCFragment &F,
                                       const MCFixup &Fixup,
                                       const MCValue &Target, uint8_t *Data,
                                       uint64_t Value, bool IsResolved) {
  if (!IsResolved)
    Asm->getWriter().recordRelocation(F, Fixup, Target, Value);

  MCFixupKind Kind = Fixup.getKind();
  if (Kind == FK_Data_1 || Kind == FK_Data_2 || Kind == FK_Data_4 ||
      Kind == FK_Data_8) {
    unsigned NumBytes = 1 << (unsigned(Kind) - unsigned(FK_Data_1));
    for (unsigned i = 0; i < NumBytes; ++i)
      Data[i] = (Value >> (i * 8)) & 0xFF;
    return;
  }

  switch (unsigned(Kind)) {
  case MicroBlaze::FIXUP_MICROBLAZE_NONE:
    break;

  case MicroBlaze::FIXUP_MICROBLAZE_32:
    // 32-bit absolute in a data word (LE).
    Data[0] = (Value >>  0) & 0xFF;
    Data[1] = (Value >>  8) & 0xFF;
    Data[2] = (Value >> 16) & 0xFF;
    Data[3] = (Value >> 24) & 0xFF;
    break;

  case MicroBlaze::FIXUP_MICROBLAZE_32_PCREL:
    // 16-bit signed PC-relative offset in bits[15:0] of a Type B instruction.
    // In LE layout, bits[15:0] are at bytes 0-1 of the instruction word.
    Data[0] = (Value >>  0) & 0xFF;
    Data[1] = (Value >>  8) & 0xFF;
    break;

  case MicroBlaze::FIXUP_MICROBLAZE_64:
  case MicroBlaze::FIXUP_MICROBLAZE_64_PCREL:
    // Two-instruction pair: IMM (at Data[0..3]) + instruction (at Data[4..7]).
    // Bits[31:16] of the symbol value go into the IMM instruction's imm16 field.
    // Bits[15:0]  go into the following instruction's imm16 field.
    // IMM instruction: bits[15:0] (LE bytes 0-1) hold the high half.
    // Following instruction: bits[15:0] (LE bytes 4-5) hold the low half.
    Data[0] = (Value >> 16) & 0xFF;
    Data[1] = (Value >> 24) & 0xFF;
    Data[4] = (Value >>  0) & 0xFF;
    Data[5] = (Value >>  8) & 0xFF;
    break;

  default:
    llvm_unreachable("Unexpected fixup kind");
  }
}

bool MicroBlazeAsmBackend::writeNopData(raw_ostream &OS, uint64_t Count,
                                         const MCSubtargetInfo *STI) const {
  if (Count % 4 != 0)
    return false;
  // NOP = or r0, r0, r0 = 0x80000000.  In LE byte order: 00 00 00 80.
  for (uint64_t i = 0; i < Count; i += 4)
    OS.write("\x00\x00\x00\x80", 4);
  return true;
}

std::unique_ptr<MCObjectTargetWriter>
MicroBlazeAsmBackend::createObjectTargetWriter() const {
  return createMicroBlazeELFObjectWriter();
}

MCAsmBackend *llvm::createMicroBlazeAsmBackend(const Target &T,
                                                const MCSubtargetInfo &STI,
                                                const MCRegisterInfo & /*MRI*/,
                                                const MCTargetOptions & /*Opts*/) {
  return new MicroBlazeAsmBackend();
}
