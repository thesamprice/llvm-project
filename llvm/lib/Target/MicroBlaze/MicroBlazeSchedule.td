//===-- MicroBlazeSchedule.td - MicroBlaze Scheduling -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Instruction itineraries for MicroBlaze (C_AREA_OPTIMIZED=0, in-order 5-stage
// pipeline). Latency values per AMD UG984 v2026.1 §5.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Functional units
//===----------------------------------------------------------------------===//

def ALU_FU  : FuncUnit; // integer ALU, shift-by-1, branches, compare
def LDST_FU : FuncUnit; // load/store pipeline
def MUL_FU  : FuncUnit; // multiplier (shared with integer divide)
def FP_FU   : FuncUnit; // floating-point unit

//===----------------------------------------------------------------------===//
// Itinerary classes — group instructions by result latency
//===----------------------------------------------------------------------===//

def IIC_ALU : InstrItinClass; // 1-cycle: add, sub, logic, compare, branch
def IIC_MUL : InstrItinClass; // 3-cycle: mul, muli, mulh, mulhu, mulhsu
def IIC_LD  : InstrItinClass; // 2-cycle: load word/half/byte
def IIC_ST  : InstrItinClass; // 1-cycle: store (no result register)
def IIC_DIV : InstrItinClass; // 34-cycle: idiv, idivu
def IIC_FPUAdd : InstrItinClass; // 6-cycle: fadd, frsub, fcmp (C_AREA_OPTIMIZED=0)
def IIC_FPUMul : InstrItinClass; // 5-cycle: fmul (C_AREA_OPTIMIZED=0)
def IIC_FPUDiv : InstrItinClass; // 32-cycle: fdiv, fsqrt (C_AREA_OPTIMIZED=0)
def IIC_SPR : InstrItinClass; // 2-cycle: mfs, mts, msrset, msrclr

//===----------------------------------------------------------------------===//
// Processor itineraries
//===----------------------------------------------------------------------===//

def MicroBlazeItinerary : ProcessorItineraries<
    [ALU_FU, LDST_FU, MUL_FU, FP_FU], [], [
  InstrItinData<IIC_ALU, [InstrStage<1,  [ALU_FU]>]>,
  InstrItinData<IIC_MUL, [InstrStage<3,  [MUL_FU]>]>,
  InstrItinData<IIC_LD,  [InstrStage<2,  [LDST_FU]>]>,
  InstrItinData<IIC_ST,  [InstrStage<1,  [LDST_FU]>]>,
  InstrItinData<IIC_DIV, [InstrStage<34, [MUL_FU]>]>,
  InstrItinData<IIC_FPUAdd, [InstrStage<6,  [FP_FU]>]>,
  InstrItinData<IIC_FPUMul, [InstrStage<5,  [FP_FU]>]>,
  InstrItinData<IIC_FPUDiv, [InstrStage<32, [FP_FU]>]>,
  InstrItinData<IIC_SPR, [InstrStage<2,  [ALU_FU]>]>,
]>;

//===----------------------------------------------------------------------===//
// Machine model
//===----------------------------------------------------------------------===//

def MicroBlazeSchedModel : SchedMachineModel {
  // 2-cycle load-to-use latency.
  let LoadLatency = 2;

  // In-order: no OOO buffer.
  let MicroOpBufferSize     = 0;
  let LoopMicroOpBufferSize = 0;

  // Single-issue scalar.
  let IssueWidth = 1;

  // Enable post-RA scheduling pass.
  let PostRAScheduler = 1;

  // Unmodeled instructions get 1-cycle default (safe for simple ALU ops).
  let CompleteModel = 0;

  let Itineraries = MicroBlazeItinerary;
}
