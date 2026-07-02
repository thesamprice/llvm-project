//===--- MicroBlaze.cpp - Implement MicroBlaze target feature support -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlaze.h"
#include "clang/Basic/Builtins.h"
#include "clang/Basic/MacroBuilder.h"
#include "clang/Basic/TargetBuiltins.h"

using namespace clang;
using namespace clang::targets;

static constexpr int NumBuiltins =
    clang::MicroBlaze::LastTSBuiltin - Builtin::FirstTSBuiltin;

static constexpr llvm::StringTable BuiltinStrings =
    CLANG_BUILTIN_STR_TABLE_START
#define BUILTIN CLANG_BUILTIN_STR_TABLE
#include "clang/Basic/BuiltinsMicroBlaze.def"
    ;

static constexpr auto BuiltinInfos = Builtin::MakeInfos<NumBuiltins>({
#define BUILTIN CLANG_BUILTIN_ENTRY
#include "clang/Basic/BuiltinsMicroBlaze.def"
});

llvm::SmallVector<Builtin::InfosShard>
MicroBlazeTargetInfo::getTargetBuiltins() const {
  return {{&BuiltinStrings, BuiltinInfos}};
}

// R0–R31 in order; ABI aliases handled by GCCRegAliases.
const char *const MicroBlazeTargetInfo::GCCRegNames[] = {
    "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",  "r8",  "r9",  "r10",
    "r11", "r12", "r13", "r14", "r15", "r16", "r17", "r18", "r19", "r20", "r21",
    "r22", "r23", "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
};

ArrayRef<const char *> MicroBlazeTargetInfo::getGCCRegNames() const {
  return llvm::ArrayRef(GCCRegNames);
}

// ABI register aliases per UG984 Ch.4.
const TargetInfo::GCCRegAlias MicroBlazeTargetInfo::GCCRegAliases[] = {
    {{"sp"}, "r1"},
    {{"rpc"}, "r15"}, // link register / return address
    {{"fp"}, "r19"},  // frame pointer (eliminable)
};

ArrayRef<TargetInfo::GCCRegAlias>
MicroBlazeTargetInfo::getGCCRegAliases() const {
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

  // PIC-level macros (consistent with other targets).
  if (Opts.PICLevel > 0) {
    Builder.defineMacro("__pic__");
    Builder.defineMacro("__PIC__");
    if (Opts.PIE) {
      Builder.defineMacro("__pie__");
      Builder.defineMacro("__PIE__");
    }
  }
}
