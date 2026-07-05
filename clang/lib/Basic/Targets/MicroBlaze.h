//===--- MicroBlaze.h - Declare MicroBlaze target feature support *- C++
//-*-===//
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
    // GCC MicroBlaze ABI aligns int64_t / double to 4 bytes, not 8.
    LongLongAlign = 32;
    DoubleAlign = 32;
    // Promote _Atomic types to their natural size alignment (up to 8 bytes).
    // Without this, _Atomic long long keeps 4-byte alignment and LLVM's
    // AtomicExpand pass fatal-errors: it requires align >= size for the
    // __atomic_load_8/__atomic_store_8 libcall path.  _Atomic double is
    // unaffected (already routed through the generic __atomic_load call at
    // the Clang IR level because it's a floating-point type).
    //
    // MaxAtomicInlineWidth = 32: correctly marks i8/i16/i32 atomics as
    // always lock-free (they are Custom-lowered inline); i64 stays at
    // "sometimes lock-free" since our __atomic_*_8 stubs use interrupt masking.
    MaxAtomicPromoteWidth = 64;
    MaxAtomicInlineWidth = 32;
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  ArrayRef<const char *> getGCCRegNames() const override;
  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override;

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::VoidPtrBuiltinVaList;
  }

  llvm::SmallVector<Builtin::InfosShard> getTargetBuiltins() const override;

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

  // The MicroBlaze ABI aligns int64_t / double to 4 bytes (see LongLongAlign /
  // DoubleAlign above and the i64:32 / f64:32 data-layout entries).  Clang's
  // default would otherwise bump the *preferred* alignment of these types to
  // their natural 8-byte size, producing `alloca align 8`.  The backend's
  // 4-byte-aligned stack frames do not honor that, so an 8-aligned alloca lets
  // the optimizer fold `&x + 4` into `&x | 4` and miscompile 64-bit stores.
  // Keep preferred alignment equal to the (4-byte) ABI alignment.
  bool allowsLargerPreferedTypeAlignment() const override { return false; }

  bool hasBitIntType() const override { return true; }
};

} // namespace targets
} // namespace clang

#endif // LLVM_CLANG_LIB_BASIC_TARGETS_MICROBLAZE_H
