//===- MicroBlaze.cpp -----------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MicroBlaze is a 32-bit RISC soft-processor from Xilinx/AMD.
// Instruction encoding is big-endian bit ordering stored in little-endian
// (microblazeel) or big-endian (microblaze) byte order.
//
// Two-instruction ("64-bit") relocations apply to an IMM+instruction pair:
//   IMM  insn: bits[15:0] carry the upper 16 bits of the full 32-bit value
//   next insn: bits[15:0] carry the lower 16 bits
// In LE byte order, bits[15:0] of a 32-bit instruction word are at bytes 0-1.
//
// PC-relative branches reference PC = address of the branch instruction, which
// is always at IMM_address + 4.  LLD computes val = S+A-P where P = IMM
// address, so the actual offset written into the pair is val - 4.
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
#include "RelocScan.h"
#include "Symbols.h"
#include "Target.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {

class MicroBlaze final : public TargetInfo {
public:
  explicit MicroBlaze(Ctx &);
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
  void scanSection(InputSectionBase &sec) override;
  template <class ELFT, class RelTy>
  void scanSectionImpl(InputSectionBase &sec, Relocs<RelTy> rels);
};

} // namespace

MicroBlaze::MicroBlaze(Ctx &ctx) : TargetInfo(ctx) {
  // NOP = or r0, r0, r0 = 0x80000000.  In LE byte order: 00 00 00 80.
  trapInstr = {0x00, 0x00, 0x00, 0x80};
}

RelExpr MicroBlaze::getRelExpr(RelType type, const Symbol &s,
                                const uint8_t *loc) const {
  switch (type) {
  case R_MICROBLAZE_NONE:
  case R_MICROBLAZE_64_NONE:
  case R_MICROBLAZE_32_NONE:
    return R_NONE;
  case R_MICROBLAZE_64_PCREL:
  case R_MICROBLAZE_32_PCREL:
    return R_PC;
  default:
    return R_ABS;
  }
}

void MicroBlaze::relocate(uint8_t *loc, const Relocation &rel,
                           uint64_t val) const {
  switch (rel.type) {
  case R_MICROBLAZE_NONE:
  case R_MICROBLAZE_64_NONE:
  case R_MICROBLAZE_32_NONE:
    break;

  case R_MICROBLAZE_32:
    write32le(loc, val);
    break;

  case R_MICROBLAZE_32_PCREL:
    // Single-word PC-relative: 32-bit value in a data location.
    write32le(loc, val);
    break;

  case R_MICROBLAZE_64: {
    // IMM+instruction pair (absolute).
    // Upper 16 bits into IMM instruction bits[15:0] (LE bytes 0-1).
    // Lower 16 bits into following instruction bits[15:0] (LE bytes 4-5).
    write16le(loc,     (val >> 16) & 0xFFFF);
    write16le(loc + 4, val & 0xFFFF);
    break;
  }

  case R_MICROBLAZE_64_PCREL: {
    // IMM+branch pair (PC-relative).
    // LLD gives val = S+A-P where P = address of IMM instruction.
    // MicroBlaze branch uses PC = IMM+4, so subtract 4 from val.
    int64_t offset = static_cast<int64_t>(val) - 4;
    write16le(loc,     (offset >> 16) & 0xFFFF);
    write16le(loc + 4, offset & 0xFFFF);
    break;
  }

  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unrecognized relocation "
             << rel.type;
  }
}

// Skip R_MICROBLAZE_NONE (type 0) before passing relocations to the generic
// scanner. The generic path checks type == iRelSymbolicRel (default: 0), so
// NONE relocations would be misclassified as ifunc-symbolic relocations.
template <class ELFT, class RelTy>
void MicroBlaze::scanSectionImpl(InputSectionBase &sec, Relocs<RelTy> rels) {
  RelocScan rs(ctx, &sec);
  sec.relocations.reserve(rels.size());
  for (auto it = rels.begin(); it != rels.end(); ++it) {
    RelType type = it->getType(false);
    if (type == R_MICROBLAZE_NONE || type == R_MICROBLAZE_64_NONE ||
        type == R_MICROBLAZE_32_NONE)
      continue;
    rs.scan<ELFT, RelTy>(it, type, rs.getAddend<ELFT>(*it, type));
  }
}

void MicroBlaze::scanSection(InputSectionBase &sec) {
  // MicroBlaze is always ELF32 little-endian (microblazeel) or big-endian.
  // Route through the concrete type so scanSectionImpl above is called instead
  // of TargetInfo::scanSectionImpl (template functions are not virtual).
  elf::scanSection1<MicroBlaze, ELF32LE>(*this, sec);
}

void elf::setMicroBlazeTargetInfo(Ctx &ctx) {
  ctx.target.reset(new MicroBlaze(ctx));
}
