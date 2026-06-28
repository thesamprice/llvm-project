//===-- MicroBlazeInstrReorder.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//   SRA    0x001 = 0b00000000001  — arithmetic right shift by 1; MSR.C ← bit[0]
//   SRC    0x021 = 0b00000100001  — shift right through carry (MSR.C in/out)
//   SRL    0x041 = 0b01000000001  — logical right shift by 1; MSR.C ← bit[0]
//   SWAPB  0x1E0 = 0b00111100000 — full 32-bit byte reversal (requires +swapb)
//   SWAPH  0x1E2 = 0b00111100010 — swap two 16-bit halfwords (requires +reorder)
//===----------------------------------------------------------------------===//
// Byte/halfword reorder instructions (opcode 0x24, UG984 §5 Figs 137-138).
// Both require C_USE_REORDER_INSTR=1 on the hardware side.
//   SWAPB  func=0x1E0 — ABCD → DCBA (full 32-bit byte reversal; implements bswap32)
//   SWAPH  func=0x1E2 — ABCD → CDAB (swap the two 16-bit halfwords; rotate-by-16)
//
// Feature hierarchy:
//   +swapb   (-mxl-swapb)   — enables SWAPB instruction only
//   +reorder (-mxl-reorder) — enables SWAPB + SWAPH (implies +swapb)
//===----------------------------------------------------------------------===//

// SWAPB: available with just +swapb (which +reorder implies).
// SWAPB alone implements bswap32 (full byte reversal ABCD→DCBA) in 1 instruction.
let Predicates = [HasSwapBytePred] in {
  def SWAPB : MBA<0x24, 0x1E0, (outs GPR:$rD), (ins GPR:$rA),
                  "swapb\t$rD, $rA", []> {
    let rB = 0;
  }

  def : Pat<(bswap i32:$rA), (SWAPB GPR:$rA)>;
}

// SWAPH requires +swaph (or +reorder which implies it).
let Predicates = [HasSwapHalfPred] in {
  def SWAPH : MBA<0x24, 0x1E2, (outs GPR:$rD), (ins GPR:$rA),
                  "swaph\t$rD, $rA", []> {
    let rB = 0;
  }
}
