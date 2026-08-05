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

  RValue EmitVAArg(CodeGenFunction &CGF, Address VAListAddr, QualType Ty,
                   AggValueSlot Slot) const override;
};
} // end anonymous namespace

ABIArgInfo MicroBlazeABIInfo::classifyArgumentType(QualType Ty) const {
  Ty = useFirstFieldIfTransparentUnion(Ty);

  // MicroBlaze has no vector hardware.  Coerce vector arguments to integer
  // words using the same [N x i32] convention as aggregates so the calling
  // convention assigns each 32-bit slot to a register (R5–R10) or stack word.
  // The large-struct compile-time cap also applies: see classifyReturnType.
  if (Ty->isVectorType()) {
    uint64_t ByteSize = getContext().getTypeSize(Ty) / 8;
    if (ByteSize > 8192)
      return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace(),
                                     /*ByVal=*/true);
    uint64_t NumWords = (ByteSize + 3) / 4;
    llvm::Type *Int32Ty = llvm::Type::getInt32Ty(getVMContext());
    llvm::Type *CoerceTy =
        NumWords == 1 ? static_cast<llvm::Type *>(Int32Ty)
                      : llvm::ArrayType::get(Int32Ty, NumWords);
    return ABIArgInfo::getDirect(CoerceTy);
  }

  if (isAggregateTypeForABI(Ty)) {
    // C++ record types may require indirect passing (e.g. non-trivial copy).
    if (const RecordType *RT = Ty->getAsCanonical<RecordType>()) {
      CGCXXABI::RecordArgABI RAA = getRecordArgABI(RT, getCXXABI());
      if (RAA == CGCXXABI::RAA_Indirect)
        return getNaturalAlignIndirectInReg(Ty);
      if (RAA == CGCXXABI::RAA_DirectInMemory)
        return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace(),
                                       /*ByVal=*/true);
      // Structs with flexible arrays must be indirect.
      if (RT->getDecl()->getDefinitionOrSelf()->hasFlexibleArrayMember())
        return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace(),
                                       /*ByVal=*/true);
    }

    // Empty records contribute nothing to the call.
    if (isEmptyRecord(getContext(), Ty, /*AllowArrays=*/true))
      return ABIArgInfo::getIgnore();

    // Pass aggregate by value as an array of 32-bit words so each word is
    // independently assigned to a register (R5–R10) or stack slot by the
    // calling-convention machine — matching GCC's MicroBlaze ABI.
    //
    // For very large aggregates, the [N x i32] coerce type creates an IR
    // function type with N arguments.  SelectionDAG's DAGCombiner passes are
    // O(N²) in the number of call-argument nodes, so structs much larger than
    // a few KB push compile time into tens of seconds (>30 s for the 64 KB
    // struct in GCC torture test pr20621-1).  Structs above 8 KiB are passed
    // indirectly (byval copy) instead.  This deviates from GCC's ABI for
    // unusually large by-value structs, but such sizes are rare in practice on
    // this bare-metal target and the compile-time cost is prohibitive.
    uint64_t ByteSize = getContext().getTypeSize(Ty) / 8;
    if (ByteSize > 8192)
      return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace(),
                                     /*ByVal=*/true);
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

  // Vectors have no hardware representation.  Small vectors (≤8 bytes) fit in
  // R3:R4 and are returned as i32 or [2 x i32]; larger vectors use sret.
  if (RetTy->isVectorType()) {
    uint64_t ByteSize = getContext().getTypeSize(RetTy) / 8;
    if (ByteSize > 8)
      return getNaturalAlignIndirect(RetTy, getDataLayout().getAllocaAddrSpace());
    uint64_t NumWords = (ByteSize + 3) / 4;
    llvm::Type *Int32Ty = llvm::Type::getInt32Ty(getVMContext());
    llvm::Type *CoerceTy =
        NumWords == 1 ? static_cast<llvm::Type *>(Int32Ty)
                      : llvm::ArrayType::get(Int32Ty, NumWords);
    return ABIArgInfo::getDirect(CoerceTy);
  }

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
    return getNaturalAlignIndirect(RetTy, getDataLayout().getAllocaAddrSpace());
  }

  if (const auto *ED = RetTy->getAsEnumDecl())
    RetTy = ED->getIntegerType();

  if (isPromotableIntegerTypeForABI(RetTy))
    return ABIArgInfo::getExtend(RetTy);

  return ABIArgInfo::getDirect();
}

RValue MicroBlazeABIInfo::EmitVAArg(CodeGenFunction &CGF, Address VAListAddr,
                                    QualType Ty, AggValueSlot Slot) const {
  // MicroBlaze passes all arguments (including aggregates) by value in 4-byte
  // slots.  va_arg must load the value directly from the slot — never via a
  // pointer — so IsIndirect=false always.
  TypeInfoChars TI = getContext().getTypeInfoInChars(Ty);
  // Slots are 4-byte aligned; round up size to a multiple of 4.
  TI.Align = std::max(TI.Align, CharUnits::fromQuantity(4));
  return emitVoidPtrVAArg(CGF, VAListAddr, Ty, /*IsIndirect=*/false,
                          TI, CharUnits::fromQuantity(4),
                          /*AllowHigherAlign=*/false, Slot);
}

namespace {
class MicroBlazeTargetCodeGenInfo : public TargetCodeGenInfo {
public:
  MicroBlazeTargetCodeGenInfo(CodeGen::CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<MicroBlazeABIInfo>(CGT)) {}

  void setTargetAttributes(const Decl *D, llvm::GlobalValue *GV,
                           CodeGen::CodeGenModule &M) const override;
};
} // end anonymous namespace

void MicroBlazeTargetCodeGenInfo::setTargetAttributes(
    const Decl *D, llvm::GlobalValue *GV, CodeGen::CodeGenModule &M) const {
  const auto *FD = dyn_cast_or_null<FunctionDecl>(D);
  if (!FD)
    return;
  auto *F = dyn_cast<llvm::Function>(GV);
  if (!F)
    return;

  // __attribute__((interrupt_handler)) / ((save_volatiles)) are callee-side
  // properties: the function preserves extra registers (and, for an interrupt,
  // MSR + rtid), but callers still call it normally.  Encoding this as a
  // function attribute — rather than a distinct calling convention — keeps the
  // call sites unchanged (a CC mismatch between a default-CC call and a
  // cc73/cc74 callee is UB and would delete the call).  MicroBlazeFrameLowering
  // keys the save/restore + return behavior off these attributes.
  if (FD->hasAttr<MicroBlazeInterruptHandlerAttr>()) {
    F->addFnAttr("interrupt-handler");
    F->addFnAttr(llvm::Attribute::NoInline);
  } else if (FD->hasAttr<MicroBlazeSaveVolatilesAttr>()) {
    F->addFnAttr("save-volatiles");
    F->addFnAttr(llvm::Attribute::NoInline);
  }
}

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createMicroBlazeTargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<MicroBlazeTargetCodeGenInfo>(CGM.getTypes());
}
