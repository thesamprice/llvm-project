//===-- MicroBlazeMCAsmInfo.h - MicroBlaze asm properties ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MCTARGETDESC_MICROBLAZEMCASMINFO_H
#define LLVM_LIB_TARGET_MICROBLAZE_MCTARGETDESC_MICROBLAZEMCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {

class MCSpecifierExpr;
class Triple;
class MCTargetOptions;

class MicroBlazeMCAsmInfo : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit MicroBlazeMCAsmInfo(const Triple &TheTriple,
                               const MCTargetOptions &Options);

  void printSpecifierExpr(raw_ostream &OS,
                          const MCSpecifierExpr &Expr) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MCTARGETDESC_MICROBLAZEMCASMINFO_H
