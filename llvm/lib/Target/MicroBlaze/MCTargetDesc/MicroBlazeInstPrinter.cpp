//===-- MicroBlazeInstPrinter.cpp - MicroBlaze MCInst to asm --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeInstPrinter.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegister.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// Pull in register and instruction enum values so the PRINT_ALIAS_INSTR block
// in MicroBlazeGenAsmWriter.inc can reference MicroBlaze::R0, MicroBlaze::ADDIK, etc.
#define GET_REGINFO_ENUM
#include "MicroBlazeGenRegisterInfoEnums.inc"
#define GET_INSTRINFO_ENUM
#include "MicroBlazeGenInstrInfo.inc"

#define PRINT_ALIAS_INSTR
#include "MicroBlazeGenAsmWriter.inc"

void MicroBlazeInstPrinter::printRegName(raw_ostream &OS, MCRegister Reg) {
  OS << getRegisterName(Reg);
}

void MicroBlazeInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                      StringRef Annot,
                                      const MCSubtargetInfo &STI,
                                      raw_ostream &O) {
  if (!printAliasInstr(MI, Address, O))
    printInstruction(MI, Address, O);
  printAnnotation(O, Annot);
}

void MicroBlazeInstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                         raw_ostream &O) {
  if (OpNo >= MI->getNumOperands()) {
    llvm::errs() << "printOperand: opcode=" << MI->getOpcode()
                 << " nops=" << MI->getNumOperands()
                 << " OpNo=" << OpNo << " out-of-bounds\n";
    return;
  }
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg()) {
    O << getRegisterName(Op.getReg());
  } else if (Op.isImm()) {
    O << Op.getImm();
  } else if (Op.isExpr()) {
    MAI.printExpr(O, *Op.getExpr());
  } else {
    llvm::errs() << "printOperand: opcode=" << MI->getOpcode()
                 << " OpNo=" << OpNo
                 << (Op.isValid() ? " unexpected-kind" : " kInvalid") << "\n";
    O << "<invalid>";
  }
}

void MicroBlazeInstPrinter::printPCRelImmOperand(const MCInst *MI,
                                                  uint64_t Address,
                                                  unsigned OpNo,
                                                  raw_ostream &O) {
  if (OpNo >= MI->getNumOperands()) {
    llvm::errs() << "printPCRelImmOperand: opcode=" << MI->getOpcode()
                 << " nops=" << MI->getNumOperands()
                 << " OpNo=" << OpNo << " out-of-bounds\n";
    return;
  }
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm())
    O << Op.getImm();
  else if (Op.isExpr())
    MAI.printExpr(O, *Op.getExpr());
  else {
    llvm::errs() << "printPCRelImmOperand: opcode=" << MI->getOpcode()
                 << " OpNo=" << OpNo
                 << (Op.isValid() ? " unexpected-kind" : " kInvalid") << "\n";
    O << "<invalid>";
  }
}

void MicroBlazeInstPrinter::printMemOperand(const MCInst *MI, unsigned OpNo,
                                             raw_ostream &O) {
  printOperand(MI, OpNo, O);
  O << ", ";
  printOperand(MI, OpNo + 1, O);
}

void MicroBlazeInstPrinter::printMemOperandRR(const MCInst *MI, unsigned OpNo,
                                               raw_ostream &O) {
  printOperand(MI, OpNo, O);
  O << ", ";
  printOperand(MI, OpNo + 1, O);
}
