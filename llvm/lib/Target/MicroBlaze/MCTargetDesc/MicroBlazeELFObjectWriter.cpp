//===-- MicroBlazeELFObjectWriter.cpp - MicroBlaze ELF Writer -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeFixupKinds.h"
#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {

class MicroBlazeELFObjectWriter : public MCELFObjectTargetWriter {
public:
  MicroBlazeELFObjectWriter()
      : MCELFObjectTargetWriter(/*Is64Bit=*/false, ELF::ELFOSABI_NONE,
                                ELF::EM_MICROBLAZE,
                                /*HasRelocationAddend=*/true) {}

  ~MicroBlazeELFObjectWriter() override = default;

protected:
  unsigned getRelocType(const MCFixup &Fixup, const MCValue &Target,
                        bool IsPCRel) const override;
};

} // end anonymous namespace

unsigned MicroBlazeELFObjectWriter::getRelocType(const MCFixup &Fixup,
                                                   const MCValue &Target,
                                                   bool IsPCRel) const {
  unsigned Kind = Fixup.getKind();
  switch (Kind) {
  case MicroBlaze::FIXUP_MICROBLAZE_NONE:
    return ELF::R_MICROBLAZE_NONE;
  case MicroBlaze::FIXUP_MICROBLAZE_32:
    return ELF::R_MICROBLAZE_32;
  case MicroBlaze::FIXUP_MICROBLAZE_32_PCREL:
    return ELF::R_MICROBLAZE_32_PCREL;
  case MicroBlaze::FIXUP_MICROBLAZE_64:
    return ELF::R_MICROBLAZE_64;
  case MicroBlaze::FIXUP_MICROBLAZE_64_PCREL:
    return ELF::R_MICROBLAZE_64_PCREL;
  case FK_Data_4:
    return IsPCRel ? ELF::R_MICROBLAZE_32_PCREL : ELF::R_MICROBLAZE_32;
  case FK_Data_8:
    return ELF::R_MICROBLAZE_64;
  default:
    llvm_unreachable("Unhandled fixup kind in getRelocType");
  }
}

std::unique_ptr<MCObjectTargetWriter>
llvm::createMicroBlazeELFObjectWriter() {
  return std::make_unique<MicroBlazeELFObjectWriter>();
}
