//===-- MicroBlazeMCAsmInfo.cpp - MicroBlaze asm properties ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeMCAsmInfo.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

void MicroBlazeMCAsmInfo::anchor() {}

MicroBlazeMCAsmInfo::MicroBlazeMCAsmInfo(const Triple & /*TheTriple*/,
                                         const MCTargetOptions &Options)
    : MCAsmInfoELF(Options) {
  IsLittleEndian = true;
  CommentString = "#";
  WeakRefDirective = "\t.weak\t";
  ExceptionsType = ExceptionHandling::DwarfCFI;
  UsesELFSectionDirectiveForBSS = true;
  SupportsDebugInformation = true;
  // All MicroBlaze instructions are 32-bit.
  MinInstAlignment = 4;
}

void MicroBlazeMCAsmInfo::printSpecifierExpr(raw_ostream &OS,
                                              const MCSpecifierExpr &Expr) const {
  printExpr(OS, *Expr.getSubExpr());
  switch (Expr.getSpecifier()) {
  case ELF::R_MICROBLAZE_GOT_64:
    OS << "@got";
    break;
  case ELF::R_MICROBLAZE_PLT_64:
    OS << "@plt";
    break;
  case ELF::R_MICROBLAZE_GOTPC_64:
    OS << "@gotpc";
    break;
  case ELF::R_MICROBLAZE_GOTOFF_64:
    OS << "@gotoff";
    break;
  default:
    OS << "@<unknown:" << Expr.getSpecifier() << ">";
    break;
  }
}
