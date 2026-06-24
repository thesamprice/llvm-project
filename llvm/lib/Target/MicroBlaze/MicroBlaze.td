//===-- MicroBlaze.td - Describe the MicroBlaze Target -----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// MicroBlaze optional ISA features
//===----------------------------------------------------------------------===//

def FeatureBarrelShift : SubtargetFeature<"barrel-shift", "HasBarrelShift",
    "true", "Enable barrel-shift instructions (bsrl, bsra, bsll)">;

def FeatureMultiplyHigh : SubtargetFeature<"multiply-high", "HasMultiplyHigh",
    "true", "Enable multiply-high instructions (mulh, mulhu, mulhsu)">;

def FeaturePatternCompare : SubtargetFeature<"pattern-compare", "HasPatternCompare",
    "true", "Enable pattern-compare instructions (cmp, cmpu)">;

def FeatureHardFloat : SubtargetFeature<"hard-float", "HasHardFloat",
    "true", "Enable hardware floating-point instructions">;

def FeatureFloatConvert : SubtargetFeature<"float-convert", "HasFloatConvert",
    "true", "Enable floating-point conversion instructions (flt, fint, fsqrt)">;

//===----------------------------------------------------------------------===//
// Register file, Calling Conv, Instruction Descriptions (added in later commits)
//===----------------------------------------------------------------------===//

include "MicroBlazeRegisterInfo.td"
include "MicroBlazeCallingConv.td"
include "MicroBlazeInstrInfo.td"

def MicroBlazeInstrInfo : InstrInfo;

//===----------------------------------------------------------------------===//
// MicroBlaze processors supported
//===----------------------------------------------------------------------===//

def : ProcessorModel<"generic", NoSchedModel, []>;
def : ProcessorModel<"v11.0", NoSchedModel,
    [FeatureBarrelShift, FeatureMultiplyHigh, FeaturePatternCompare]>;

def MicroBlazeInstPrinter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
  bit isMCAsmWriter = 1;
}

//===----------------------------------------------------------------------===//
// Declare the target which we are implementing
//===----------------------------------------------------------------------===//

def MicroBlaze : Target {
  let InstructionSet = MicroBlazeInstrInfo;
  let AssemblyWriters = [MicroBlazeInstPrinter];
}
