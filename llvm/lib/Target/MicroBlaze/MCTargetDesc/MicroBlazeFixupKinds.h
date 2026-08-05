//===-- MicroBlazeFixupKinds.h - MicroBlaze Fixup Entries -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MCTARGETDESC_MICROBLAZEFIXUPKINDS_H
#define LLVM_LIB_TARGET_MICROBLAZE_MCTARGETDESC_MICROBLAZEFIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace MicroBlaze {

// Fixup kinds map 1-to-1 to ELF relocation types (R_MICROBLAZE_*).
// The linker applies these to patch instruction immediate fields or data words.
// UG984 Ch.5 instruction formats; ELF reloc values from the MicroBlaze ELF ABI.
enum Fixups {
  // No relocation; placeholder.
  FIXUP_MICROBLAZE_NONE = FirstTargetFixupKind,

  // R_MICROBLAZE_32 — absolute 32-bit data relocation.
  FIXUP_MICROBLAZE_32,

  // R_MICROBLAZE_32_PCREL — single-instruction PC-relative (short branch).
  FIXUP_MICROBLAZE_32_PCREL,

  // R_MICROBLAZE_64 — two-instruction absolute (IMM + Type-B pair).
  // Applied at the IMM instruction; linker patches both words.
  FIXUP_MICROBLAZE_64,

  // R_MICROBLAZE_64_PCREL — two-instruction PC-relative (IMM + branch/call).
  // Applied at the IMM instruction; linker patches both words.
  FIXUP_MICROBLAZE_64_PCREL,

  // R_MICROBLAZE_GOT_64 — two-instruction GOT-relative (IMM + lwi r20 pair).
  // Applied at the IMM instruction; linker fills in GOT entry offset.
  FIXUP_MICROBLAZE_GOT_64,

  // R_MICROBLAZE_PLT_64 — two-instruction PLT-relative (IMM + branch pair).
  // Applied at the IMM instruction; linker creates/patches PLT stub.
  FIXUP_MICROBLAZE_PLT_64,

  // R_MICROBLAZE_GOTOFF_64 — two-instruction offset from the GOT base
  // (IMM + addik r20 pair).  A link-time constant; used for PIC jump tables.
  FIXUP_MICROBLAZE_GOTOFF_64,

  // Marker.
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};

} // namespace MicroBlaze
} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MCTARGETDESC_MICROBLAZEFIXUPKINDS_H
