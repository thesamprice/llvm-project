//===- MicroBlaze.cpp -----------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"

using namespace clang;
using namespace clang::CodeGen;

//===----------------------------------------------------------------------===//
// MicroBlaze ABI Implementation
//
// MicroBlaze ILP32 little-endian ABI (as implemented by GCC for this target):
//   - Argument registers: R5–R10 (6 × 32-bit slots).
//   - Additional arguments spill to the caller's outgoing stack area.
//   - Struct/union arguments are passed BY VALUE, treating the aggregate as an
//     array of 32-bit words: words fill R5–R10 in argument order, then stack.
//   - Return values: i32/f32 in R3; i64/f64 in R3 (lo) : R4 (hi).
//   - Large struct returns use a hidden first-arg pointer (sret).
//
// The calling-convention machine in MicroBlazeCallingConv.td only sees i32
// scalars; this ABI info coerces aggregates to [N x i32] so that the word-
// by-word assignment to registers and stack happens naturally.
//===----------------------------------------------------------------------===//

namespace {
class MicroBlazeABIInfo : public DefaultABIInfo {
public:
  MicroBlazeABIInfo(CodeGen::CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}

  ABIArgInfo classifyArgumentType(QualType Ty) const;
  ABIArgInfo classifyReturnType(QualType RetTy) const;

  void computeInfo(CGFunctionInfo &FI) const override {
    if (!getCXXABI().classifyReturnType(FI))
      FI.getReturnInfo() = classifyReturnType(FI.getReturnType());
    for (auto &I : FI.arguments())
      I.info = classifyArgumentType(I.type);
  }
};
} // end anonymous namespace

ABIArgInfo MicroBlazeABIInfo::classifyArgumentType(QualType Ty) const {
  Ty = useFirstFieldIfTransparentUnion(Ty);

  if (isAggregateTypeForABI(Ty)) {
    // C++ record types may require indirect passing (e.g. non-trivial copy).
    if (const RecordType *RT = Ty->getAsCanonical<RecordType>()) {
      CGCXXABI::RecordArgABI RAA = getRecordArgABI(RT, getCXXABI());
      if (RAA == CGCXXABI::RAA_Indirect)
        return getNaturalAlignIndirectInReg(Ty);
      if (RAA == CGCXXABI::RAA_DirectInMemory)
        return getNaturalAlignIndirect(Ty,
                                       getDataLayout().getAllocaAddrSpace(),
                                       /*ByVal=*/true);
      // Structs with flexible arrays must be indirect.
      if (RT->getDecl()->getDefinitionOrSelf()->hasFlexibleArrayMember())
        return getNaturalAlignIndirect(Ty,
                                       getDataLayout().getAllocaAddrSpace(),
                                       /*ByVal=*/true);
    }

    // Empty records contribute nothing to the call.
    if (isEmptyRecord(getContext(), Ty, /*AllowArrays=*/true))
      return ABIArgInfo::getIgnore();

    // Pass aggregate by value as an array of 32-bit words so each word is
    // independently assigned to a register (R5–R10) or stack slot by the
    // calling-convention machine — matching GCC's MicroBlaze ABI.
    uint64_t ByteSize = getContext().getTypeSize(Ty) / 8;
    uint64_t NumWords = (ByteSize + 3) / 4;
    llvm::Type *Int32Ty = llvm::Type::getInt32Ty(getVMContext());
    llvm::Type *CoerceTy = llvm::ArrayType::get(Int32Ty, NumWords);
    return ABIArgInfo::getDirect(CoerceTy);
  }

  // Treat enums as their underlying integer type.
  if (const auto *ED = Ty->getAsEnumDecl())
    Ty = ED->getIntegerType();

  if (isPromotableIntegerTypeForABI(Ty))
    return ABIArgInfo::getExtend(Ty);

  return ABIArgInfo::getDirect();
}

ABIArgInfo MicroBlazeABIInfo::classifyReturnType(QualType RetTy) const {
  if (RetTy->isVoidType())
    return ABIArgInfo::getIgnore();

  if (isAggregateTypeForABI(RetTy)) {
    // Small aggregates (≤8 bytes) fit in R3:R4 and are coerced to [N x i32].
    uint64_t ByteSize = getContext().getTypeSize(RetTy) / 8;
    if (ByteSize <= 8) {
      uint64_t NumWords = (ByteSize + 3) / 4;
      llvm::Type *Int32Ty = llvm::Type::getInt32Ty(getVMContext());
      llvm::Type *CoerceTy = llvm::ArrayType::get(Int32Ty, NumWords);
      return ABIArgInfo::getDirect(CoerceTy);
    }
    // Large aggregates: return via hidden sret pointer (first argument).
    return getNaturalAlignIndirect(RetTy,
                                   getDataLayout().getAllocaAddrSpace());
  }

  if (const auto *ED = RetTy->getAsEnumDecl())
    RetTy = ED->getIntegerType();

  if (isPromotableIntegerTypeForABI(RetTy))
    return ABIArgInfo::getExtend(RetTy);

  return ABIArgInfo::getDirect();
}

namespace {
class MicroBlazeTargetCodeGenInfo : public TargetCodeGenInfo {
public:
  MicroBlazeTargetCodeGenInfo(CodeGen::CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<MicroBlazeABIInfo>(CGT)) {}
};
} // end anonymous namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createMicroBlazeTargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<MicroBlazeTargetCodeGenInfo>(CGM.getTypes());
}
