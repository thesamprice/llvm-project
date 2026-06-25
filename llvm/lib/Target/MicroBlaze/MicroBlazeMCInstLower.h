//===-- MicroBlazeMCInstLower.h - Lower MachineInstr to MCInst ---*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEMCINSTLOWER_H
#define LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEMCINSTLOWER_H

#include "llvm/Support/Compiler.h"

namespace llvm {

class AsmPrinter;
class MachineInstr;
class MachineOperand;
class MCContext;
class MCInst;
class MCOperand;

class MicroBlazeMCInstLower {
  MCContext &Ctx;
  AsmPrinter &Printer;

public:
  MicroBlazeMCInstLower(MCContext &Ctx, AsmPrinter &Printer);
  void Lower(const MachineInstr *MI, MCInst &OutMI) const;

private:
  MCOperand LowerOperand(const MachineOperand &MO, int64_t Offset = 0) const;
  MCOperand LowerSymbolOperand(const MachineOperand &MO) const;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEMCINSTLOWER_H
