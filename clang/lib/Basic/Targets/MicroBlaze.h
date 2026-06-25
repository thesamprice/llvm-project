//===--- MicroBlaze.h - Declare MicroBlaze target feature support *- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_MICROBLAZE_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_MICROBLAZE_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"

namespace clang {
namespace targets {

class LLVM_LIBRARY_VISIBILITY MicroBlazeTargetInfo : public TargetInfo {
  static const char *const GCCRegNames[];
  static const TargetInfo::GCCRegAlias GCCRegAliases[];

public:
  MicroBlazeTargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple) {
    // ILP32: int=long=pointer=32 bits.
    // Data layout is kept in sync with computeMicroBlazeDataLayout() in
    // llvm/lib/TargetParser/TargetDataLayout.cpp.
    resetDataLayout();
    SizeType = UnsignedInt;
    PtrDiffType = SignedInt;
    IntPtrType = SignedInt;
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  ArrayRef<const char *> getGCCRegNames() const override;
  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override;

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::VoidPtrBuiltinVaList;
  }

  llvm::SmallVector<Builtin::InfosShard> getTargetBuiltins() const override {
    return {};
  }

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &Info) const override {
    switch (*Name) {
    case 'r': // GPR
      Info.setAllowsRegister();
      return true;
    default:
      return false;
    }
  }

  std::string_view getClobbers() const override { return ""; }

  bool hasBitIntType() const override { return true; }
};

} // namespace targets
} // namespace clang

#endif // LLVM_CLANG_LIB_BASIC_TARGETS_MICROBLAZE_H
