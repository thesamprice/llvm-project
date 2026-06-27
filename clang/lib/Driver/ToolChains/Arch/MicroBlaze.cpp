//===--- MicroBlaze.cpp - MicroBlaze-specific driver helpers --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Maps GCC-compatible -mxl-* driver flags to LLVM target features so that
// the same compiler flags work with both GCC and clang:
//
//   GCC flag                LLVM feature
//   --------------------    ----------------
//   -mxl-barrel-shift       +barrel-shift
//   -mno-xl-barrel-shift    -barrel-shift
//   -mxl-pattern-compare    +pattern-compare
//   -mno-xl-pattern-compare -pattern-compare
//   -mno-xl-soft-div        +divide
//   -mxl-soft-div           -divide
//   -mxl-multiply-high      +multiply-high
//   -mno-xl-multiply-high   -multiply-high
//   -mxl-reorder            +reorder
//   -mno-xl-reorder         -reorder
//   -mno-xl-soft-float      +hard-float  (C_USE_FPU=1; enables HW FPU)
//   -mxl-soft-float         -hard-float  (C_USE_FPU=0; software emulation, default)
//   -mxl-float-convert      +float-convert  (C_USE_FPU=2; flt/fint/fsqrt)
//   -mno-xl-float-convert   -float-convert
//
//===----------------------------------------------------------------------===//

#include "MicroBlaze.h"
#include "clang/Options/Options.h"
#include "llvm/Option/ArgList.h"

using namespace clang::driver;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

void microblaze::getMicroBlazeTargetFeatures(
    const Driver &D, const ArgList &Args,
    std::vector<StringRef> &Features) {

  if (Arg *A = Args.getLastArg(options::OPT_mxl_barrel_shift,
                               options::OPT_mno_xl_barrel_shift))
    Features.push_back(A->getOption().matches(options::OPT_mxl_barrel_shift)
                           ? "+barrel-shift"
                           : "-barrel-shift");

  if (Arg *A = Args.getLastArg(options::OPT_mxl_pattern_compare,
                               options::OPT_mno_xl_pattern_compare))
    Features.push_back(
        A->getOption().matches(options::OPT_mxl_pattern_compare)
            ? "+pattern-compare"
            : "-pattern-compare");

  // GCC default is software divide (-mxl-soft-div); -mno-xl-soft-div enables
  // hardware divide, mapping to +divide.
  if (Arg *A = Args.getLastArg(options::OPT_mno_xl_soft_div,
                               options::OPT_mxl_soft_div))
    Features.push_back(
        A->getOption().matches(options::OPT_mno_xl_soft_div) ? "+divide"
                                                              : "-divide");

  if (Arg *A = Args.getLastArg(options::OPT_mxl_multiply_high,
                               options::OPT_mno_xl_multiply_high))
    Features.push_back(
        A->getOption().matches(options::OPT_mxl_multiply_high)
            ? "+multiply-high"
            : "-multiply-high");

  if (Arg *A = Args.getLastArg(options::OPT_mxl_reorder,
                               options::OPT_mno_xl_reorder))
    Features.push_back(A->getOption().matches(options::OPT_mxl_reorder)
                           ? "+reorder"
                           : "-reorder");

  // -mno-xl-soft-float enables hardware FPU (C_USE_FPU=1 → +hard-float).
  // -mxl-soft-float requests software emulation (default, → -hard-float).
  if (Arg *A = Args.getLastArg(options::OPT_mno_xl_soft_float,
                               options::OPT_mxl_soft_float))
    Features.push_back(
        A->getOption().matches(options::OPT_mno_xl_soft_float) ? "+hard-float"
                                                               : "-hard-float");

  // Extended FPU: hardware float/int conversion instructions (flt, fint,
  // fsqrt; C_USE_FPU=2).  Gated independently of +hard-float, matching GCC's
  // separate -mxl-float-convert flag; combine with -mno-xl-soft-float for a
  // full extended FPU.
  if (Arg *A = Args.getLastArg(options::OPT_mxl_float_convert,
                               options::OPT_mno_xl_float_convert))
    Features.push_back(
        A->getOption().matches(options::OPT_mxl_float_convert)
            ? "+float-convert"
            : "-float-convert");
}
