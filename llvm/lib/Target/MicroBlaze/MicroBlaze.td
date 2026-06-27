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

def FeatureDivide : SubtargetFeature<"divide", "HasDivide",
    "true", "Enable hardware integer divide instructions (idiv, idivu)">;

def FeatureReorderInstr : SubtargetFeature<"reorder", "HasReorderInstr",
    "true", "Enable byte/halfword reorder instructions (swapb, swaph)">;

def FeatureHardFloat : SubtargetFeature<"hard-float", "HasHardFloat",
    "true", "Enable hardware floating-point instructions">;

def FeatureFloatConvert : SubtargetFeature<"float-convert", "HasFloatConvert",
    "true", "Enable floating-point conversion instructions (flt, fint, fsqrt)">;

//===----------------------------------------------------------------------===//
// Register file, Calling Conv, Instruction Descriptions (added in later commits)
//===----------------------------------------------------------------------===//

include "MicroBlazeRegisterInfo.td"
include "MicroBlazeCallingConv.td"
include "MicroBlazeSchedule.td"
include "MicroBlazeInstrInfo.td"

def MicroBlazeInstrInfo : InstrInfo;

// Map the generic ptr_rc (pointer-like register class used by standard
// pseudoinstructions) to the MicroBlaze GPR class.
defm : RemapAllTargetPseudoPointerOperands<GPR>;

//===----------------------------------------------------------------------===//
// MicroBlaze processors supported
//===----------------------------------------------------------------------===//

// generic: matches the default hardware configuration shipped by AMD/Xilinx,
// which includes the pattern-compare unit (cmp/cmpu) in all standard builds.
def : ProcessorModel<"generic", MicroBlazeSchedModel, [FeaturePatternCompare]>;
def : ProcessorModel<"v11.0", MicroBlazeSchedModel,
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
