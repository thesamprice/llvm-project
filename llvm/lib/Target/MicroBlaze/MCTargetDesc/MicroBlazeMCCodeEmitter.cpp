//===-- MicroBlazeMCCodeEmitter.cpp - MicroBlaze instruction encoding -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeMCTargetDesc.h"
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

using namespace llvm;

namespace {

class MicroBlazeMCCodeEmitter : public MCCodeEmitter {
  const MCInstrInfo &MCII;
  const MCRegisterInfo &MRI;

  uint64_t getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  uint64_t getMemOpValue(const MCInst &MI, unsigned OpNo,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const;

  uint64_t getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

public:
  MicroBlazeMCCodeEmitter(const MCInstrInfo &mcii, const MCRegisterInfo &mri)
      : MCII(mcii), MRI(mri) {}

  void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;
};

} // namespace

// Include the generated encoding table (defines getBinaryCodeForInstr).
#include "MicroBlazeGenMCCodeEmitter.inc"

uint64_t MicroBlazeMCCodeEmitter::getMachineOpValue(
    const MCInst &MI, const MCOperand &MO, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  if (MO.isReg())
    return MRI.getEncodingValue(MO.getReg());
  if (MO.isImm())
    return static_cast<uint64_t>(MO.getImm());
  assert(MO.isExpr());
  Fixups.push_back(MCFixup::create(0, MO.getExpr(), FK_Data_4));
  return 0;
}

uint64_t MicroBlazeMCCodeEmitter::getMemOpValue(
    const MCInst &MI, unsigned OpNo, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  // memri: base register in bits[20:16], offset in bits[15:0].
  uint64_t Base = MRI.getEncodingValue(MI.getOperand(OpNo).getReg());
  uint64_t Offset = static_cast<uint64_t>(MI.getOperand(OpNo + 1).getImm());
  return (Base << 16) | (Offset & 0xFFFF);
}

void MicroBlazeMCCodeEmitter::encodeInstruction(
    const MCInst &MI, SmallVectorImpl<char> &CB,
    SmallVectorImpl<MCFixup> &Fixups, const MCSubtargetInfo &STI) const {
  uint32_t Bits = static_cast<uint32_t>(getBinaryCodeForInstr(MI, Fixups, STI));
  // MicroBlaze instructions are 32-bit; emit little-endian.
  support::endian::write<uint32_t>(CB, Bits, llvm::endianness::little);
}

namespace llvm {

MCCodeEmitter *createMicroBlazeMCCodeEmitter(const MCInstrInfo &MCII,
                                              MCContext &Ctx) {
  return new MicroBlazeMCCodeEmitter(MCII, *Ctx.getRegisterInfo());
}

} // namespace llvm
