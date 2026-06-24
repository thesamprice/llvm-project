//===-- MicroBlazeMCAsmInfo.cpp - MicroBlaze asm properties ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeMCAsmInfo.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

void MicroBlazeMCAsmInfo::anchor() {}

MicroBlazeMCAsmInfo::MicroBlazeMCAsmInfo(const Triple &TheTriple,
                                         const MCTargetOptions &Options)
    : MCAsmInfoELF(Options) {
  IsLittleEndian = TheTriple.isLittleEndian();
  CommentString = "#";
  WeakRefDirective = "\t.weak\t";
  ExceptionsType = ExceptionHandling::DwarfCFI;
  UsesELFSectionDirectiveForBSS = true;
  SupportsDebugInformation = true;
  // All MicroBlaze instructions are 32-bit.
  MinInstAlignment = 4;
}
