//===--- MicroBlaze.cpp - Implement MicroBlaze target feature support -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlaze.h"
#include "clang/Basic/MacroBuilder.h"

using namespace clang;
using namespace clang::targets;

// R0–R31 in order; ABI aliases handled by GCCRegAliases.
const char *const MicroBlazeTargetInfo::GCCRegNames[] = {
    "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",
    "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
};

ArrayRef<const char *> MicroBlazeTargetInfo::getGCCRegNames() const {
  return llvm::ArrayRef(GCCRegNames);
}

// ABI register aliases per UG984 Ch.4.
const TargetInfo::GCCRegAlias MicroBlazeTargetInfo::GCCRegAliases[] = {
    {{"sp"}, "r1"},
    {{"rpc"}, "r15"},  // link register / return address
    {{"fp"}, "r19"},   // frame pointer (eliminable)
};

ArrayRef<TargetInfo::GCCRegAlias> MicroBlazeTargetInfo::getGCCRegAliases() const {
  return llvm::ArrayRef(GCCRegAliases);
}

void MicroBlazeTargetInfo::getTargetDefines(const LangOptions &Opts,
                                             MacroBuilder &Builder) const {
  Builder.defineMacro("__MICROBLAZE__");
  Builder.defineMacro("__microblaze__");
  Builder.defineMacro("__mb__");

  // Endianness macro — always LE for microblazeel.
  if (getTriple().getArch() == llvm::Triple::microblazeel)
    Builder.defineMacro("__MICROBLAZEEL__");
  else
    Builder.defineMacro("__MICROBLAZEEB__");
}
