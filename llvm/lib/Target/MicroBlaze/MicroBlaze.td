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

// FeatureSwapByte: enables the SWAPB instruction (swap bytes within each halfword).
// FeatureSwapHalf: enables the SWAPH instruction (swap two 16-bit halfwords).
//   SWAPH implies SWAPB so both are usable together for 2-instruction bswap32.
// Both require C_USE_REORDER_INSTR=1 on the hardware.
// FeatureReorderInstr remains the combined user-facing flag (implies both).
def FeatureSwapByte : SubtargetFeature<"swapb", "HasSwapByte",
    "true", "Enable byte-within-halfword swap instruction (swapb)">;

def FeatureSwapHalf : SubtargetFeature<"swaph", "HasSwapHalf",
    "true", "Enable halfword-swap instruction (swaph); implies swapb",
    [FeatureSwapByte]>;

def FeatureReorderInstr : SubtargetFeature<"reorder", "HasReorderInstr",
    "true", "Enable byte/halfword reorder instructions (swapb, swaph)",
    [FeatureSwapByte, FeatureSwapHalf]>;

def FeatureHardFloat : SubtargetFeature<"hard-float", "HasHardFloat",
    "true", "Enable hardware floating-point instructions">;

def FeatureFloatConvert : SubtargetFeature<"float-convert", "HasFloatConvert",
    "true", "Enable floating-point conversion instructions (flt, fint, fsqrt)">;

def FeatureExtendedAddr : SubtargetFeature<"extended-addr", "HasExtendedAddr",
    "true", "Enable extended-address memory instructions (lbuea, lhuea, lwea, sbea, shea, swea)">;

def FeatureFSL : SubtargetFeature<"fsl", "HasFSL",
    "true", "Enable FSL/AXI-Stream stream interface instructions (get, put, getd, putd, and variants)">;

// C_AREA_OPTIMIZED=1: the 3-stage area pipeline.  Selects MicroBlazeAreaModel for
// scheduling (barrel-shift and multiply become multi-cycle); see
// MicroBlazeSchedule.td and MicroBlazeSubtarget.cpp.
def FeatureAreaOptimized : SubtargetFeature<"area-optimized", "HasAreaOptimized",
    "true", "Schedule for the 3-stage area-optimized pipeline (C_AREA_OPTIMIZED=1)">;

// C_USE_BRANCH_TARGET_CACHE=1: assume a branch target cache is present.  With a
// BTC, a correctly predicted immediate branch or return incurs no refill
// overhead (UG984 Ch.2 "Branch Target Cache"), so the delay-slot filler prefers
// demoting an unfilled immediate branch over keeping the D-form + a wasted NOP.
// Only valid on the 5-/8-stage pipeline (the BTC does not exist with
// C_AREA_OPTIMIZED=1); see MicroBlazeDelaySlotFiller.cpp preferNopOverDemote.
def FeatureBranchTargetCache : SubtargetFeature<"branch-target-cache",
    "HasBranchTargetCache", "true",
    "Assume a branch target cache is present (C_USE_BRANCH_TARGET_CACHE=1)">;

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
def : ProcessorModel<"generic", MicroBlazeSpeedModel, [FeaturePatternCompare]>;
def : ProcessorModel<"v11.0", MicroBlazeSpeedModel,
    [FeatureBarrelShift, FeatureMultiplyHigh, FeaturePatternCompare,
     FeatureDivide]>;

// Internal tune-CPU used only to select MicroBlazeAreaModel for scheduling when
// the +area-optimized feature is requested (MicroBlazeSubtarget passes this as
// the TuneCPU).  It is feature-orthogonal: the real CPU/-mattr set still applies.
def : ProcessorModel<"mb-area", MicroBlazeAreaModel, []>;

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
