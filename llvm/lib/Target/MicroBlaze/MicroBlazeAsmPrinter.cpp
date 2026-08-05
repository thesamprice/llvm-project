//===-- MicroBlazeAsmPrinter.cpp - MicroBlaze Assembly Printer ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/MicroBlazeInstPrinter.h"
#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "MicroBlazeMCInstLower.h"
#include "MicroBlazeSubtarget.h"
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

  // The delay-slot filler inserts a NOP (non-terminator) between the branch
  // and the next real terminator, which breaks getFirstTerminator()'s backward
  // scan and causes the base class to incorrectly classify a branch-target
  // block as fall-through-only, suppressing its label.  Always emit labels.
  bool
  isBlockOnlyReachableByFallthrough(const MachineBasicBlock *) const override {
    return false;
  }

  StringRef getPassName() const override {
    return "MicroBlaze Assembly Printer";
  }

  void emitInstruction(const MachineInstr *MI) override;
  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &O) override;
};

} // namespace

void MicroBlazeAsmPrinter::emitInstruction(const MachineInstr *MI) {
  MicroBlazeMCInstLower MCInstLowering(OutContext, *this);

  // Emit the instruction and any bundle body instructions (delay slot fillers).
  // The base AsmPrinter loop uses bundle_iterator, which visits only bundle
  // heads; we must emit bundle body instructions inline here.
  MachineBasicBlock::const_instr_iterator I = MI->getIterator();
  MachineBasicBlock::const_instr_iterator E = MI->getParent()->instr_end();
  do {
    MCInst TmpInst;
    MCInstLowering.Lower(&*I, TmpInst);
    EmitToStreamer(*OutStreamer, TmpInst);
  } while ((++I != E) && I->isInsideBundle());
}

bool MicroBlazeAsmPrinter::PrintAsmOperand(const MachineInstr *MI,
                                           unsigned OpNo, const char *ExtraCode,
                                           raw_ostream &O) {
  if (ExtraCode && ExtraCode[0])
    return AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, O);
  const MachineOperand &MO = MI->getOperand(OpNo);
  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    O << MicroBlazeInstPrinter::getRegisterName(MO.getReg());
    return false;
  case MachineOperand::MO_Immediate:
    O << MO.getImm();
    return false;
  default:
    return AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, O);
  }
}

// Force static initialisation.
extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMicroBlazeAsmPrinter() {
  RegisterAsmPrinter<MicroBlazeAsmPrinter> X(getTheMicroBlazeELTarget());
}
