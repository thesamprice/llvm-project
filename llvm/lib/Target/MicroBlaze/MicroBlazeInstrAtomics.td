//===-- MicroBlazeInstrAtomics.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// LWX Rd, Ra, Rb — load word exclusive (opcode=0x32, func=0x400). UG984 §5 Fig 103.
// Loads word from rA+rB, sets reservation bit; sets MSR[C] on exclusive-access failure.
// Latency: 1 or 2 cycles (C_AREA_OPTIMIZED=1).
def LWX  : MBA<0x32, 0x400, (outs GPR:$rD), (ins GPR:$rA, GPR:$rB),
               "lwx\t$rD, $rA, $rB", []> {
  let hasSideEffects = 1;
  let mayLoad = 1;
}

// SWX Rd, Ra, Rb — store word exclusive (opcode=0x36, func=0x400). UG984 §5 Fig 140.
// Conditional store: succeeds only when reservation bit is set; clears reservation.
// Sets MSR[C]=1 if store is suppressed, MSR[C]=0 on success.
// Latency: 1 or 2 cycles (C_AREA_OPTIMIZED=1).
def SWX : MBA<0x36, 0x400, (outs), (ins GPR:$rD, GPR:$rA, GPR:$rB),
              "swx\t$rD, $rA, $rB", []> {
  let hasSideEffects = 1;
  let mayStore = 1;
}

