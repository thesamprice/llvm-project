//===-- MicroBlazeAsmPrinter.cpp - MicroBlaze Assembly Printer ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeMCInstLower.h"
#include "MicroBlazeSubtarget.h"
#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "TargetInfo/MicroBlazeTargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

class MicroBlazeAsmPrinter : public AsmPrinter {
public:
  explicit MicroBlazeAsmPrinter(TargetMachine &TM,
                                 std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "MicroBlaze Assembly Printer"; }

  void emitInstruction(const MachineInstr *MI) override;
};

} // namespace

void MicroBlazeAsmPrinter::emitInstruction(const MachineInstr *MI) {
  MicroBlazeMCInstLower MCInstLowering(OutContext, *this);
  MCInst TmpInst;
  MCInstLowering.Lower(MI, TmpInst);
  EmitToStreamer(*OutStreamer, TmpInst);
}

// Force static initialisation.
extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMicroBlazeAsmPrinter() {
  RegisterAsmPrinter<MicroBlazeAsmPrinter> X(getTheMicroBlazeELTarget());
}
