//===-- MicroBlazeSchedule.td - MicroBlaze Scheduling -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MicroBlaze scheduling, modern per-instruction SchedRW form.  Every
// instruction carries a SchedRW list (see MicroBlazeInstrFormats.td); there are
// no ProcessorItineraries.
//
// Two pipeline configurations are modelled, selected by the +area-optimized
// feature (UG984 "MicroBlaze Architecture" pipeline section):
//   * MicroBlazeSpeedModel — C_AREA_OPTIMIZED=0, 5-stage performance pipeline.
//     Multi-cycle (non-pipelined) categories: divide, floating-point.
//   * MicroBlazeAreaModel  — C_AREA_OPTIMIZED=1, 3-stage area pipeline.
//     Multi-cycle categories additionally include barrel-shift and multiply
//     (UG984: the 3-stage pipeline's multi-cycle set is barrel shift, multiply,
//     divide, and floating-point), so those serialize on their unit.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Target-level SchedWrites — one per instruction latency class, shared by both
// machine models (the per-config latencies live in the WriteRes blocks below).
//===----------------------------------------------------------------------===//

def WriteALU    : SchedWrite; // add, sub, logic, compare, sext, single-bit shift
def WriteBarrel : SchedWrite; // barrel shift (bsll/bsra/bsrl, bsefi/bsifi)
def WriteMul    : SchedWrite; // mul, muli, mulh, mulhu, mulhsu
def WriteLd     : SchedWrite; // load word/half/byte
def WriteSt     : SchedWrite; // store (no result register)
def WriteDiv    : SchedWrite; // idiv, idivu
def WriteFPUAdd : SchedWrite; // fadd, frsub, fcmp
def WriteFPUMul : SchedWrite; // fmul
def WriteFPUDiv : SchedWrite; // fdiv, fsqrt
def WriteSPR    : SchedWrite; // mfs, mts, msrset, msrclr
def WriteMBAR   : SchedWrite; // memory barrier
def WriteFSL    : SchedWrite; // FSL/AXI-Stream get/put

//===----------------------------------------------------------------------===//
// Performance model (C_AREA_OPTIMIZED=0, 5-stage)
//===----------------------------------------------------------------------===//

def MicroBlazeSpeedModel : SchedMachineModel {
  let LoadLatency           = 2;   // 2-cycle load-to-use
  let MicroOpBufferSize     = 0;   // in-order, no OOO buffer
  let LoopMicroOpBufferSize = 0;
  let IssueWidth            = 1;   // single-issue scalar
  let PostRAScheduler       = 1;
  let CompleteModel         = 1;
}

let SchedModel = MicroBlazeSpeedModel in {
  // In-order issue: every resource is unbuffered (BufferSize = 0).
  def SpeedALU  : ProcResource<1> { let BufferSize = 0; }
  def SpeedLdSt : ProcResource<1> { let BufferSize = 0; }
  def SpeedMul  : ProcResource<1> { let BufferSize = 0; } // shared with integer divide
  def SpeedFPU  : ProcResource<1> { let BufferSize = 0; }

  def : WriteRes<WriteALU,    [SpeedALU]>  { let Latency = 1; }
  def : WriteRes<WriteBarrel, [SpeedALU]>  { let Latency = 1; } // pipelined in 5-stage
  def : WriteRes<WriteSPR,    [SpeedALU]>  { let Latency = 2; }
  def : WriteRes<WriteMBAR,   [SpeedALU]>  { let Latency = 1; }
  def : WriteRes<WriteFSL,    [SpeedALU]>  { let Latency = 1; }
  def : WriteRes<WriteLd,     [SpeedLdSt]> { let Latency = 2; }
  def : WriteRes<WriteSt,     [SpeedLdSt]> { let Latency = 1; }
  def : WriteRes<WriteMul,    [SpeedMul]>  { let Latency = 3; } // pipelined (1/cycle)
  def : WriteRes<WriteDiv,    [SpeedMul]>  { let Latency = 34; let ReleaseAtCycles = [34]; }
  def : WriteRes<WriteFPUAdd, [SpeedFPU]>  { let Latency = 6; }
  def : WriteRes<WriteFPUMul, [SpeedFPU]>  { let Latency = 5; }
  def : WriteRes<WriteFPUDiv, [SpeedFPU]>  { let Latency = 32; let ReleaseAtCycles = [32]; }

  // Register copies lower to an ALU add (addk rd, rs, r0).
  def : InstRW<[WriteALU], (instrs COPY)>;
}

//===----------------------------------------------------------------------===//
// Area model (C_AREA_OPTIMIZED=1, 3-stage)
//
// The 3-stage pipeline reuses hardware across cycles, so barrel-shift and
// multiply become multi-cycle: they hold their unit (ReleaseAtCycles = Latency)
// and so serialize against following ops, unlike the pipelined 5-stage versions.
//===----------------------------------------------------------------------===//

def MicroBlazeAreaModel : SchedMachineModel {
  let LoadLatency           = 2;
  let MicroOpBufferSize     = 0;
  let LoopMicroOpBufferSize = 0;
  let IssueWidth            = 1;
  let PostRAScheduler       = 1;
  let CompleteModel         = 1;
}

let SchedModel = MicroBlazeAreaModel in {
  def AreaALU  : ProcResource<1> { let BufferSize = 0; }
  def AreaLdSt : ProcResource<1> { let BufferSize = 0; }
  def AreaMul  : ProcResource<1> { let BufferSize = 0; }
  def AreaFPU  : ProcResource<1> { let BufferSize = 0; }

  def : WriteRes<WriteALU,    [AreaALU]>  { let Latency = 1; }
  def : WriteRes<WriteBarrel, [AreaALU]>  { let Latency = 2; let ReleaseAtCycles = [2]; } // multi-cycle
  def : WriteRes<WriteSPR,    [AreaALU]>  { let Latency = 2; }
  def : WriteRes<WriteMBAR,   [AreaALU]>  { let Latency = 1; }
  def : WriteRes<WriteFSL,    [AreaALU]>  { let Latency = 1; }
  def : WriteRes<WriteLd,     [AreaLdSt]> { let Latency = 2; }
  def : WriteRes<WriteSt,     [AreaLdSt]> { let Latency = 1; }
  def : WriteRes<WriteMul,    [AreaMul]>  { let Latency = 3; let ReleaseAtCycles = [3]; } // non-pipelined
  def : WriteRes<WriteDiv,    [AreaMul]>  { let Latency = 34; let ReleaseAtCycles = [34]; }
  def : WriteRes<WriteFPUAdd, [AreaFPU]>  { let Latency = 6; }
  def : WriteRes<WriteFPUMul, [AreaFPU]>  { let Latency = 5; let ReleaseAtCycles = [5]; }
  def : WriteRes<WriteFPUDiv, [AreaFPU]>  { let Latency = 32; let ReleaseAtCycles = [32]; }

  // Register copies lower to an ALU add (addk rd, rs, r0).
  def : InstRW<[WriteALU], (instrs COPY)>;
}
